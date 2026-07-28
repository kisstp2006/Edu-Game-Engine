#include "ScriptCoreHelpers.h"

#include "ScriptMath.h"

#include <angelscript.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <iomanip>
#include <limits>
#include <new>
#include <random>
#include <sstream>
#include <string>
#include <string_view>

namespace EGE
{
	namespace
	{
		constexpr std::int64_t TicksPerMillisecond = 10'000;
		constexpr std::int64_t TicksPerSecond = 10'000'000;
		constexpr std::int64_t TicksPerMinute = TicksPerSecond * 60;
		constexpr std::int64_t TicksPerHour = TicksPerMinute * 60;
		constexpr std::int64_t TicksPerDay = TicksPerHour * 24;

		std::int64_t ClampTicks(long double ticks)
		{
			return static_cast<std::int64_t>(std::clamp(
				ticks,
				static_cast<long double>(
					std::numeric_limits<std::int64_t>::min()),
				static_cast<long double>(
					std::numeric_limits<std::int64_t>::max())));
		}

		std::int64_t SaturatingAdd(
			std::int64_t left,
			std::int64_t right)
		{
			if (right > 0 &&
				left > std::numeric_limits<std::int64_t>::max() - right)
			{
				return std::numeric_limits<std::int64_t>::max();
			}
			if (right < 0 &&
				left < std::numeric_limits<std::int64_t>::min() - right)
			{
				return std::numeric_limits<std::int64_t>::min();
			}
			return left + right;
		}

		std::int64_t SaturatingSubtract(
			std::int64_t left,
			std::int64_t right)
		{
			if (right > 0 &&
				left < std::numeric_limits<std::int64_t>::min() + right)
			{
				return std::numeric_limits<std::int64_t>::min();
			}
			if (right < 0 &&
				left > std::numeric_limits<std::int64_t>::max() + right)
			{
				return std::numeric_limits<std::int64_t>::max();
			}
			return left - right;
		}

		std::uint32_t CreateRandomSeed()
		{
			static std::atomic<std::uint32_t> sequence{0};
			std::random_device device;
			const auto clock = std::chrono::steady_clock::now()
				.time_since_epoch().count();
			return device() ^
				static_cast<std::uint32_t>(clock) ^
				++sequence;
		}

		std::string FormatNumber(double value, int precision)
		{
			std::ostringstream stream;
			stream << std::setprecision(precision) << value;
			return stream.str();
		}

		template <typename Type>
		void DefaultConstruct(Type* value)
		{
			new (value) Type();
		}

		template <typename Type>
		void CopyConstruct(const Type& source, Type* value)
		{
			new (value) Type(source);
		}

		template <typename Type>
		void Destruct(Type* value)
		{
			value->~Type();
		}

		template <typename Type>
		Type& Assign(const Type& source, Type* value)
		{
			*value = source;
			return *value;
		}

		class ScriptRandom final
		{
		public:
			ScriptRandom()
				: engine_(CreateRandomSeed())
			{
			}

			explicit ScriptRandom(int seed)
				: engine_(static_cast<std::uint32_t>(seed))
			{
			}

			int Next()
			{
				return std::uniform_int_distribution<int>(
					0, std::numeric_limits<int>::max() - 1)(engine_);
			}

			int Next(int maximum)
			{
				if (maximum <= 0)
					return 0;
				return std::uniform_int_distribution<int>(
					0, maximum - 1)(engine_);
			}

			int Next(int minimum, int maximum)
			{
				if (maximum <= minimum)
					return minimum;
				return std::uniform_int_distribution<int>(
					minimum, maximum - 1)(engine_);
			}

			double NextDouble()
			{
				return std::generate_canonical<double, 53>(engine_);
			}

			float NextSingle()
			{
				return std::generate_canonical<float, 24>(engine_);
			}

			bool NextBool()
			{
				return std::uniform_int_distribution<int>(0, 1)(engine_) != 0;
			}

			ScriptVector3 NextVector3(float minimum, float maximum)
			{
				if (maximum < minimum)
					std::swap(minimum, maximum);
				std::uniform_real_distribution<float> distribution(
					minimum, maximum);
				return {
					distribution(engine_),
					distribution(engine_),
					distribution(engine_)};
			}

			ScriptColor NextColor(float alpha)
			{
				return {
					NextSingle(),
					NextSingle(),
					NextSingle(),
					std::clamp(alpha, 0.0f, 1.0f)};
			}

		private:
			std::mt19937 engine_;
		};

		void ConstructRandomWithSeed(int seed, ScriptRandom* value)
		{
			new (value) ScriptRandom(seed);
		}

		class ScriptStringBuilder final
		{
		public:
			ScriptStringBuilder() = default;

			explicit ScriptStringBuilder(const std::string& value)
				: value_(value)
			{
			}

			ScriptStringBuilder& Append(const std::string& value)
			{
				value_ += value;
				return *this;
			}

			ScriptStringBuilder& Append(int value)
			{
				value_ += std::to_string(value);
				return *this;
			}

			ScriptStringBuilder& Append(unsigned int value)
			{
				value_ += std::to_string(value);
				return *this;
			}

			ScriptStringBuilder& Append(std::int64_t value)
			{
				value_ += std::to_string(value);
				return *this;
			}

			ScriptStringBuilder& Append(std::uint64_t value)
			{
				value_ += std::to_string(value);
				return *this;
			}

			ScriptStringBuilder& Append(float value)
			{
				value_ += FormatNumber(value, 7);
				return *this;
			}

			ScriptStringBuilder& Append(double value)
			{
				value_ += FormatNumber(value, 15);
				return *this;
			}

			ScriptStringBuilder& Append(bool value)
			{
				value_ += value ? "True" : "False";
				return *this;
			}

			ScriptStringBuilder& AppendLine()
			{
				value_ += '\n';
				return *this;
			}

			ScriptStringBuilder& AppendLine(const std::string& value)
			{
				value_ += value;
				value_ += '\n';
				return *this;
			}

			ScriptStringBuilder& Clear()
			{
				value_.clear();
				return *this;
			}

			ScriptStringBuilder& Insert(
				unsigned int index,
				const std::string& value)
			{
				value_.insert(
					std::min<std::size_t>(index, value_.size()), value);
				return *this;
			}

			ScriptStringBuilder& Remove(
				unsigned int startIndex,
				unsigned int length)
			{
				if (startIndex < value_.size())
					value_.erase(startIndex, length);
				return *this;
			}

			ScriptStringBuilder& Replace(
				const std::string& oldValue,
				const std::string& newValue)
			{
				if (oldValue.empty())
					return *this;

				std::size_t position = 0;
				while ((position = value_.find(oldValue, position)) !=
					std::string::npos)
				{
					value_.replace(position, oldValue.size(), newValue);
					position += newValue.size();
				}
				return *this;
			}

			unsigned int Length() const
			{
				return static_cast<unsigned int>(value_.size());
			}

			std::string ToString() const
			{
				return value_;
			}

		private:
			std::string value_;
		};

		void ConstructStringBuilder(
			const std::string& value,
			ScriptStringBuilder* builder)
		{
			new (builder) ScriptStringBuilder(value);
		}

		struct ScriptTimeSpan
		{
			std::int64_t ticks;

			std::int64_t Add(const ScriptTimeSpan& other) const
			{
				return SaturatingAdd(ticks, other.ticks);
			}

			std::int64_t Subtract(const ScriptTimeSpan& other) const
			{
				return SaturatingSubtract(ticks, other.ticks);
			}

			std::int64_t Duration() const
			{
				if (ticks == std::numeric_limits<std::int64_t>::min())
					return std::numeric_limits<std::int64_t>::max();
				return ticks < 0 ? -ticks : ticks;
			}

			std::int64_t Negate() const
			{
				if (ticks == std::numeric_limits<std::int64_t>::min())
					return std::numeric_limits<std::int64_t>::max();
				return -ticks;
			}

			int Compare(const ScriptTimeSpan& other) const
			{
				return ticks < other.ticks ? -1 : ticks > other.ticks ? 1 : 0;
			}

			bool Equals(const ScriptTimeSpan& other) const
			{
				return ticks == other.ticks;
			}

			int Days() const
			{
				return static_cast<int>(ticks / TicksPerDay);
			}

			int Hours() const
			{
				return static_cast<int>((ticks / TicksPerHour) % 24);
			}

			int Minutes() const
			{
				return static_cast<int>((ticks / TicksPerMinute) % 60);
			}

			int Seconds() const
			{
				return static_cast<int>((ticks / TicksPerSecond) % 60);
			}

			int Milliseconds() const
			{
				return static_cast<int>(
					(ticks / TicksPerMillisecond) % 1000);
			}

			double TotalDays() const
			{
				return static_cast<double>(ticks) / TicksPerDay;
			}

			double TotalHours() const
			{
				return static_cast<double>(ticks) / TicksPerHour;
			}

			double TotalMinutes() const
			{
				return static_cast<double>(ticks) / TicksPerMinute;
			}

			double TotalSeconds() const
			{
				return static_cast<double>(ticks) / TicksPerSecond;
			}

			double TotalMilliseconds() const
			{
				return static_cast<double>(ticks) / TicksPerMillisecond;
			}

			std::int64_t Ticks() const
			{
				return ticks;
			}

			std::string ToString() const
			{
				const bool negative = ticks < 0;
				const std::uint64_t magnitude = negative
					? static_cast<std::uint64_t>(-(ticks + 1)) + 1
					: static_cast<std::uint64_t>(ticks);
				const std::uint64_t days = magnitude / TicksPerDay;
				const std::uint64_t hours =
					(magnitude / TicksPerHour) % 24;
				const std::uint64_t minutes =
					(magnitude / TicksPerMinute) % 60;
				const std::uint64_t seconds =
					(magnitude / TicksPerSecond) % 60;
				const std::uint64_t milliseconds =
					(magnitude / TicksPerMillisecond) % 1000;

				std::ostringstream stream;
				if (negative)
					stream << '-';
				if (days > 0)
					stream << days << '.';
				stream
					<< std::setfill('0')
					<< std::setw(2) << hours << ':'
					<< std::setw(2) << minutes << ':'
					<< std::setw(2) << seconds << '.'
					<< std::setw(3) << milliseconds;
				return stream.str();
			}
		};

		void ConstructTimeSpanTicks(
			std::int64_t ticks,
			ScriptTimeSpan* value)
		{
			new (value) ScriptTimeSpan{ticks};
		}

		void ConstructTimeSpanHms(
			int hours,
			int minutes,
			int seconds,
			ScriptTimeSpan* value)
		{
			const long double ticks =
				static_cast<long double>(hours) * TicksPerHour +
				static_cast<long double>(minutes) * TicksPerMinute +
				static_cast<long double>(seconds) * TicksPerSecond;
			new (value) ScriptTimeSpan{ClampTicks(ticks)};
		}

		std::int64_t TicksFromDouble(double value, double ticksPerUnit)
		{
			if (!std::isfinite(value))
				return 0;
			const long double ticks =
				static_cast<long double>(value) * ticksPerUnit;
			return ClampTicks(ticks);
		}

		std::int64_t TimeSpanFromDays(double value)
		{
			return TicksFromDouble(value, TicksPerDay);
		}

		std::int64_t TimeSpanFromHours(double value)
		{
			return TicksFromDouble(value, TicksPerHour);
		}

		std::int64_t TimeSpanFromMinutes(double value)
		{
			return TicksFromDouble(value, TicksPerMinute);
		}

		std::int64_t TimeSpanFromSeconds(double value)
		{
			return TicksFromDouble(value, TicksPerSecond);
		}

		std::int64_t TimeSpanFromMilliseconds(double value)
		{
			return TicksFromDouble(value, TicksPerMillisecond);
		}

		const ScriptTimeSpan TimeSpanZero{};

		class ScriptStopwatch final
		{
		public:
			void Start()
			{
				if (running_)
					return;
				startedAt_ = Clock::now();
				running_ = true;
			}

			void Stop()
			{
				if (!running_)
					return;
				elapsed_ += Clock::now() - startedAt_;
				running_ = false;
			}

			void Reset()
			{
				elapsed_ = Clock::duration::zero();
				running_ = false;
			}

			void Restart()
			{
				elapsed_ = Clock::duration::zero();
				startedAt_ = Clock::now();
				running_ = true;
			}

			bool IsRunning() const
			{
				return running_;
			}

			std::int64_t Elapsed() const
			{
				const auto nanoseconds =
					std::chrono::duration_cast<std::chrono::nanoseconds>(
						ElapsedDuration()).count();
				return nanoseconds / 100;
			}

			std::int64_t ElapsedMilliseconds() const
			{
				return Elapsed() / TicksPerMillisecond;
			}

			double ElapsedSeconds() const
			{
				return static_cast<double>(Elapsed()) / TicksPerSecond;
			}

		private:
			using Clock = std::chrono::steady_clock;

			Clock::duration ElapsedDuration() const
			{
				return elapsed_ +
					(running_ ? Clock::now() - startedAt_
							  : Clock::duration::zero());
			}

			Clock::time_point startedAt_{};
			Clock::duration elapsed_{};
			bool running_ = false;
		};

		ScriptStopwatch StartNewStopwatch()
		{
			ScriptStopwatch stopwatch;
			stopwatch.Start();
			return stopwatch;
		}

		struct ScriptGuid
		{
			std::uint64_t high = 0;
			std::uint64_t low = 0;

			bool Equals(const ScriptGuid& other) const
			{
				return high == other.high && low == other.low;
			}

			bool IsEmpty() const
			{
				return high == 0 && low == 0;
			}

			std::string ToString() const
			{
				std::array<unsigned char, 16> bytes{};
				for (int index = 0; index < 8; ++index)
				{
					bytes[index] = static_cast<unsigned char>(
						high >> ((7 - index) * 8));
					bytes[index + 8] = static_cast<unsigned char>(
						low >> ((7 - index) * 8));
				}

				std::ostringstream stream;
				stream << std::hex << std::setfill('0');
				for (std::size_t index = 0; index < bytes.size(); ++index)
				{
					if (index == 4 || index == 6 ||
						index == 8 || index == 10)
					{
						stream << '-';
					}
					stream << std::setw(2)
						<< static_cast<unsigned int>(bytes[index]);
				}
				return stream.str();
			}
		};

		int HexDigit(char character)
		{
			if (character >= '0' && character <= '9')
				return character - '0';
			if (character >= 'a' && character <= 'f')
				return character - 'a' + 10;
			if (character >= 'A' && character <= 'F')
				return character - 'A' + 10;
			return -1;
		}

		bool TryParseGuid(
			const std::string& text,
			ScriptGuid& result)
		{
			std::array<unsigned char, 16> bytes{};
			std::size_t nibble = 0;
			for (char character : text)
			{
				if (character == '-' || character == '{' ||
					character == '}' || character == '(' ||
					character == ')')
				{
					continue;
				}

				const int digit = HexDigit(character);
				if (digit < 0 || nibble >= bytes.size() * 2)
					return false;
				if ((nibble & 1) == 0)
					bytes[nibble / 2] =
						static_cast<unsigned char>(digit << 4);
				else
					bytes[nibble / 2] |=
						static_cast<unsigned char>(digit);
				++nibble;
			}
			if (nibble != bytes.size() * 2)
				return false;

			ScriptGuid parsed;
			for (int index = 0; index < 8; ++index)
			{
				parsed.high = (parsed.high << 8) | bytes[index];
				parsed.low = (parsed.low << 8) | bytes[index + 8];
			}
			result = parsed;
			return true;
		}

		ScriptGuid ParseGuid(const std::string& text)
		{
			ScriptGuid value;
			TryParseGuid(text, value);
			return value;
		}

		ScriptGuid NewGuid()
		{
			thread_local std::mt19937_64 engine(CreateRandomSeed());
			ScriptGuid value{engine(), engine()};
			value.high =
				(value.high & 0xffffffffffff0fffULL) |
				0x0000000000004000ULL;
			value.low =
				(value.low & 0x3fffffffffffffffULL) |
				0x8000000000000000ULL;
			return value;
		}

		const ScriptGuid GuidEmpty{};

		bool StringContains(
			const std::string* value,
			const std::string& search)
		{
			return value->find(search) != std::string::npos;
		}

		bool StringStartsWith(
			const std::string* value,
			const std::string& prefix)
		{
			return value->starts_with(prefix);
		}

		bool StringEndsWith(
			const std::string* value,
			const std::string& suffix)
		{
			return value->ends_with(suffix);
		}

		std::string StringToUpper(const std::string* value)
		{
			std::string result = *value;
			std::transform(
				result.begin(), result.end(), result.begin(),
				[](unsigned char character)
				{
					return static_cast<char>(std::toupper(character));
				});
			return result;
		}

		std::string StringToLower(const std::string* value)
		{
			std::string result = *value;
			std::transform(
				result.begin(), result.end(), result.begin(),
				[](unsigned char character)
				{
					return static_cast<char>(std::tolower(character));
				});
			return result;
		}

		std::string StringTrim(const std::string* value)
		{
			std::string_view view(*value);
			while (!view.empty() &&
				std::isspace(
					static_cast<unsigned char>(view.front())))
			{
				view.remove_prefix(1);
			}
			while (!view.empty() &&
				std::isspace(
					static_cast<unsigned char>(view.back())))
			{
				view.remove_suffix(1);
			}
			return std::string(view);
		}

		std::string StringReplace(
			const std::string* value,
			const std::string& oldValue,
			const std::string& newValue)
		{
			ScriptStringBuilder builder(*value);
			builder.Replace(oldValue, newValue);
			return builder.ToString();
		}

		unsigned int StringLength(const std::string* value)
		{
			return static_cast<unsigned int>(value->size());
		}

		bool IsNullOrEmpty(const std::string& value)
		{
			return value.empty();
		}

		bool IsNullOrWhiteSpace(const std::string& value)
		{
			return std::all_of(
				value.begin(), value.end(),
				[](unsigned char character)
				{
					return std::isspace(character) != 0;
				});
		}

		int CompareOrdinal(
			const std::string& left,
			const std::string& right)
		{
			return left < right ? -1 : left > right ? 1 : 0;
		}

		std::string PathCombine(
			const std::string& left,
			const std::string& right)
		{
			return (std::filesystem::path(left) / right)
				.lexically_normal().generic_string();
		}

		std::string PathGetFileName(const std::string& path)
		{
			return std::filesystem::path(path).filename().string();
		}

		std::string PathGetFileNameWithoutExtension(
			const std::string& path)
		{
			return std::filesystem::path(path).stem().string();
		}

		std::string PathGetExtension(const std::string& path)
		{
			return std::filesystem::path(path).extension().string();
		}

		std::string PathGetDirectoryName(const std::string& path)
		{
			return std::filesystem::path(path)
				.parent_path().generic_string();
		}

		std::string PathChangeExtension(
			const std::string& path,
			const std::string& extension)
		{
			std::filesystem::path result(path);
			result.replace_extension(extension);
			return result.generic_string();
		}

		bool PathHasExtension(const std::string& path)
		{
			return std::filesystem::path(path).has_extension();
		}

		bool RegisterRandom(asIScriptEngine& engine)
		{
			return
				engine.RegisterObjectType(
					"Random", sizeof(ScriptRandom),
					asOBJ_VALUE | asGetTypeTraits<ScriptRandom>()) >= 0 &&
				engine.RegisterObjectBehaviour(
					"Random", asBEHAVE_CONSTRUCT, "void f()",
					asFUNCTION(DefaultConstruct<ScriptRandom>),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectBehaviour(
					"Random", asBEHAVE_CONSTRUCT, "void f(int seed)",
					asFUNCTION(ConstructRandomWithSeed),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectBehaviour(
					"Random", asBEHAVE_CONSTRUCT,
					"void f(const Random &in)",
					asFUNCTION(CopyConstruct<ScriptRandom>),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectBehaviour(
					"Random", asBEHAVE_DESTRUCT, "void f()",
					asFUNCTION(Destruct<ScriptRandom>),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"Random", "Random &opAssign(const Random &in)",
					asFUNCTION(Assign<ScriptRandom>),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"Random", "int Next()",
					asMETHODPR(ScriptRandom, Next, (), int),
					asCALL_THISCALL) >= 0 &&
				engine.RegisterObjectMethod(
					"Random", "int Next(int maxValue)",
					asMETHODPR(ScriptRandom, Next, (int), int),
					asCALL_THISCALL) >= 0 &&
				engine.RegisterObjectMethod(
					"Random", "int Next(int minValue, int maxValue)",
					asMETHODPR(ScriptRandom, Next, (int, int), int),
					asCALL_THISCALL) >= 0 &&
				engine.RegisterObjectMethod(
					"Random", "double NextDouble()",
					asMETHOD(ScriptRandom, NextDouble),
					asCALL_THISCALL) >= 0 &&
				engine.RegisterObjectMethod(
					"Random", "float NextSingle()",
					asMETHOD(ScriptRandom, NextSingle),
					asCALL_THISCALL) >= 0 &&
				engine.RegisterObjectMethod(
					"Random", "bool NextBool()",
					asMETHOD(ScriptRandom, NextBool),
					asCALL_THISCALL) >= 0 &&
				engine.RegisterObjectMethod(
					"Random",
					"Vector3 NextVector3(float minValue = -1, "
						"float maxValue = 1)",
					asMETHOD(ScriptRandom, NextVector3),
					asCALL_THISCALL) >= 0 &&
				engine.RegisterObjectMethod(
					"Random", "Color NextColor(float alpha = 1)",
					asMETHOD(ScriptRandom, NextColor),
					asCALL_THISCALL) >= 0;
		}

		bool RegisterStringBuilder(asIScriptEngine& engine)
		{
			return
				engine.RegisterObjectType(
					"StringBuilder", sizeof(ScriptStringBuilder),
					asOBJ_VALUE |
						asGetTypeTraits<ScriptStringBuilder>()) >= 0 &&
				engine.RegisterObjectBehaviour(
					"StringBuilder", asBEHAVE_CONSTRUCT, "void f()",
					asFUNCTION(DefaultConstruct<ScriptStringBuilder>),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectBehaviour(
					"StringBuilder", asBEHAVE_CONSTRUCT,
					"void f(const string &in value)",
					asFUNCTION(ConstructStringBuilder),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectBehaviour(
					"StringBuilder", asBEHAVE_CONSTRUCT,
					"void f(const StringBuilder &in)",
					asFUNCTION(CopyConstruct<ScriptStringBuilder>),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectBehaviour(
					"StringBuilder", asBEHAVE_DESTRUCT, "void f()",
					asFUNCTION(Destruct<ScriptStringBuilder>),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"StringBuilder",
					"StringBuilder &opAssign(const StringBuilder &in)",
					asFUNCTION(Assign<ScriptStringBuilder>),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"StringBuilder",
					"StringBuilder &Append(const string &in value)",
					asMETHODPR(
						ScriptStringBuilder, Append,
						(const std::string&), ScriptStringBuilder&),
					asCALL_THISCALL) >= 0 &&
				engine.RegisterObjectMethod(
					"StringBuilder", "StringBuilder &Append(int value)",
					asMETHODPR(
						ScriptStringBuilder, Append,
						(int), ScriptStringBuilder&),
					asCALL_THISCALL) >= 0 &&
				engine.RegisterObjectMethod(
					"StringBuilder", "StringBuilder &Append(uint value)",
					asMETHODPR(
						ScriptStringBuilder, Append,
						(unsigned int), ScriptStringBuilder&),
					asCALL_THISCALL) >= 0 &&
				engine.RegisterObjectMethod(
					"StringBuilder", "StringBuilder &Append(int64 value)",
					asMETHODPR(
						ScriptStringBuilder, Append,
						(std::int64_t), ScriptStringBuilder&),
					asCALL_THISCALL) >= 0 &&
				engine.RegisterObjectMethod(
					"StringBuilder", "StringBuilder &Append(uint64 value)",
					asMETHODPR(
						ScriptStringBuilder, Append,
						(std::uint64_t), ScriptStringBuilder&),
					asCALL_THISCALL) >= 0 &&
				engine.RegisterObjectMethod(
					"StringBuilder", "StringBuilder &Append(float value)",
					asMETHODPR(
						ScriptStringBuilder, Append,
						(float), ScriptStringBuilder&),
					asCALL_THISCALL) >= 0 &&
				engine.RegisterObjectMethod(
					"StringBuilder", "StringBuilder &Append(double value)",
					asMETHODPR(
						ScriptStringBuilder, Append,
						(double), ScriptStringBuilder&),
					asCALL_THISCALL) >= 0 &&
				engine.RegisterObjectMethod(
					"StringBuilder", "StringBuilder &Append(bool value)",
					asMETHODPR(
						ScriptStringBuilder, Append,
						(bool), ScriptStringBuilder&),
					asCALL_THISCALL) >= 0 &&
				engine.RegisterObjectMethod(
					"StringBuilder", "StringBuilder &AppendLine()",
					asMETHODPR(
						ScriptStringBuilder, AppendLine,
						(), ScriptStringBuilder&),
					asCALL_THISCALL) >= 0 &&
				engine.RegisterObjectMethod(
					"StringBuilder",
					"StringBuilder &AppendLine(const string &in value)",
					asMETHODPR(
						ScriptStringBuilder, AppendLine,
						(const std::string&), ScriptStringBuilder&),
					asCALL_THISCALL) >= 0 &&
				engine.RegisterObjectMethod(
					"StringBuilder", "StringBuilder &Clear()",
					asMETHOD(ScriptStringBuilder, Clear),
					asCALL_THISCALL) >= 0 &&
				engine.RegisterObjectMethod(
					"StringBuilder",
					"StringBuilder &Insert(uint index, "
						"const string &in value)",
					asMETHOD(ScriptStringBuilder, Insert),
					asCALL_THISCALL) >= 0 &&
				engine.RegisterObjectMethod(
					"StringBuilder",
					"StringBuilder &Remove(uint startIndex, uint length)",
					asMETHOD(ScriptStringBuilder, Remove),
					asCALL_THISCALL) >= 0 &&
				engine.RegisterObjectMethod(
					"StringBuilder",
					"StringBuilder &Replace(const string &in oldValue, "
						"const string &in newValue)",
					asMETHOD(ScriptStringBuilder, Replace),
					asCALL_THISCALL) >= 0 &&
				engine.RegisterObjectMethod(
					"StringBuilder", "uint get_Length() const property",
					asMETHOD(ScriptStringBuilder, Length),
					asCALL_THISCALL) >= 0 &&
				engine.RegisterObjectMethod(
					"StringBuilder", "string ToString() const",
					asMETHOD(ScriptStringBuilder, ToString),
					asCALL_THISCALL) >= 0;
		}

		bool RegisterTimeSpan(asIScriptEngine& engine)
		{
			const bool typeRegistered =
				engine.RegisterObjectType(
					"TimeSpan", sizeof(ScriptTimeSpan),
					asOBJ_VALUE | asOBJ_POD | asOBJ_APP_PRIMITIVE) >= 0 &&
				engine.RegisterObjectBehaviour(
					"TimeSpan", asBEHAVE_CONSTRUCT, "void f()",
					asFUNCTION(DefaultConstruct<ScriptTimeSpan>),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectBehaviour(
					"TimeSpan", asBEHAVE_CONSTRUCT,
					"void f(int64 ticks)",
					asFUNCTION(ConstructTimeSpanTicks),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectBehaviour(
					"TimeSpan", asBEHAVE_CONSTRUCT,
					"void f(int hours, int minutes, int seconds)",
					asFUNCTION(ConstructTimeSpanHms),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"TimeSpan", "TimeSpan &opAssign(const TimeSpan &in)",
					asFUNCTION(Assign<ScriptTimeSpan>),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"TimeSpan",
					"TimeSpan opAdd(const TimeSpan &in other) const",
					asMETHOD(ScriptTimeSpan, Add),
					asCALL_THISCALL) >= 0 &&
				engine.RegisterObjectMethod(
					"TimeSpan",
					"TimeSpan opSub(const TimeSpan &in other) const",
					asMETHOD(ScriptTimeSpan, Subtract),
					asCALL_THISCALL) >= 0 &&
				engine.RegisterObjectMethod(
					"TimeSpan",
					"bool opEquals(const TimeSpan &in other) const",
					asMETHOD(ScriptTimeSpan, Equals),
					asCALL_THISCALL) >= 0 &&
				engine.RegisterObjectMethod(
					"TimeSpan",
					"int opCmp(const TimeSpan &in other) const",
					asMETHOD(ScriptTimeSpan, Compare),
					asCALL_THISCALL) >= 0 &&
				engine.RegisterObjectMethod(
					"TimeSpan", "TimeSpan Duration() const",
					asMETHOD(ScriptTimeSpan, Duration),
					asCALL_THISCALL) >= 0 &&
				engine.RegisterObjectMethod(
					"TimeSpan", "TimeSpan Negate() const",
					asMETHOD(ScriptTimeSpan, Negate),
					asCALL_THISCALL) >= 0 &&
				engine.RegisterObjectMethod(
					"TimeSpan", "int get_Days() const property",
					asMETHOD(ScriptTimeSpan, Days),
					asCALL_THISCALL) >= 0 &&
				engine.RegisterObjectMethod(
					"TimeSpan", "int get_Hours() const property",
					asMETHOD(ScriptTimeSpan, Hours),
					asCALL_THISCALL) >= 0 &&
				engine.RegisterObjectMethod(
					"TimeSpan", "int get_Minutes() const property",
					asMETHOD(ScriptTimeSpan, Minutes),
					asCALL_THISCALL) >= 0 &&
				engine.RegisterObjectMethod(
					"TimeSpan", "int get_Seconds() const property",
					asMETHOD(ScriptTimeSpan, Seconds),
					asCALL_THISCALL) >= 0 &&
				engine.RegisterObjectMethod(
					"TimeSpan", "int get_Milliseconds() const property",
					asMETHOD(ScriptTimeSpan, Milliseconds),
					asCALL_THISCALL) >= 0 &&
				engine.RegisterObjectMethod(
					"TimeSpan", "double get_TotalDays() const property",
					asMETHOD(ScriptTimeSpan, TotalDays),
					asCALL_THISCALL) >= 0 &&
				engine.RegisterObjectMethod(
					"TimeSpan", "double get_TotalHours() const property",
					asMETHOD(ScriptTimeSpan, TotalHours),
					asCALL_THISCALL) >= 0 &&
				engine.RegisterObjectMethod(
					"TimeSpan", "double get_TotalMinutes() const property",
					asMETHOD(ScriptTimeSpan, TotalMinutes),
					asCALL_THISCALL) >= 0 &&
				engine.RegisterObjectMethod(
					"TimeSpan", "double get_TotalSeconds() const property",
					asMETHOD(ScriptTimeSpan, TotalSeconds),
					asCALL_THISCALL) >= 0 &&
				engine.RegisterObjectMethod(
					"TimeSpan",
					"double get_TotalMilliseconds() const property",
					asMETHOD(ScriptTimeSpan, TotalMilliseconds),
					asCALL_THISCALL) >= 0 &&
				engine.RegisterObjectMethod(
					"TimeSpan", "int64 get_Ticks() const property",
					asMETHOD(ScriptTimeSpan, Ticks),
					asCALL_THISCALL) >= 0 &&
				engine.RegisterObjectMethod(
					"TimeSpan", "string ToString() const",
					asMETHOD(ScriptTimeSpan, ToString),
					asCALL_THISCALL) >= 0;
			if (!typeRegistered)
				return false;

			engine.SetDefaultNamespace("TimeSpan");
			const bool factoriesRegistered =
				engine.RegisterGlobalProperty(
					"const TimeSpan Zero",
					const_cast<ScriptTimeSpan*>(&TimeSpanZero)) >= 0 &&
				engine.RegisterGlobalFunction(
					"TimeSpan FromDays(double value)",
					asFUNCTION(TimeSpanFromDays), asCALL_CDECL) >= 0 &&
				engine.RegisterGlobalFunction(
					"TimeSpan FromHours(double value)",
					asFUNCTION(TimeSpanFromHours), asCALL_CDECL) >= 0 &&
				engine.RegisterGlobalFunction(
					"TimeSpan FromMinutes(double value)",
					asFUNCTION(TimeSpanFromMinutes), asCALL_CDECL) >= 0 &&
				engine.RegisterGlobalFunction(
					"TimeSpan FromSeconds(double value)",
					asFUNCTION(TimeSpanFromSeconds), asCALL_CDECL) >= 0 &&
				engine.RegisterGlobalFunction(
					"TimeSpan FromMilliseconds(double value)",
					asFUNCTION(TimeSpanFromMilliseconds),
					asCALL_CDECL) >= 0;
			engine.SetDefaultNamespace("");
			return factoriesRegistered;
		}

		bool RegisterStopwatch(asIScriptEngine& engine)
		{
			const bool typeRegistered =
				engine.RegisterObjectType(
					"Stopwatch", sizeof(ScriptStopwatch),
					asOBJ_VALUE |
						asGetTypeTraits<ScriptStopwatch>()) >= 0 &&
				engine.RegisterObjectBehaviour(
					"Stopwatch", asBEHAVE_CONSTRUCT, "void f()",
					asFUNCTION(DefaultConstruct<ScriptStopwatch>),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectBehaviour(
					"Stopwatch", asBEHAVE_CONSTRUCT,
					"void f(const Stopwatch &in)",
					asFUNCTION(CopyConstruct<ScriptStopwatch>),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectBehaviour(
					"Stopwatch", asBEHAVE_DESTRUCT, "void f()",
					asFUNCTION(Destruct<ScriptStopwatch>),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"Stopwatch",
					"Stopwatch &opAssign(const Stopwatch &in)",
					asFUNCTION(Assign<ScriptStopwatch>),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"Stopwatch", "void Start()",
					asMETHOD(ScriptStopwatch, Start),
					asCALL_THISCALL) >= 0 &&
				engine.RegisterObjectMethod(
					"Stopwatch", "void Stop()",
					asMETHOD(ScriptStopwatch, Stop),
					asCALL_THISCALL) >= 0 &&
				engine.RegisterObjectMethod(
					"Stopwatch", "void Reset()",
					asMETHOD(ScriptStopwatch, Reset),
					asCALL_THISCALL) >= 0 &&
				engine.RegisterObjectMethod(
					"Stopwatch", "void Restart()",
					asMETHOD(ScriptStopwatch, Restart),
					asCALL_THISCALL) >= 0 &&
				engine.RegisterObjectMethod(
					"Stopwatch", "bool get_IsRunning() const property",
					asMETHOD(ScriptStopwatch, IsRunning),
					asCALL_THISCALL) >= 0 &&
				engine.RegisterObjectMethod(
					"Stopwatch", "TimeSpan get_Elapsed() const property",
					asMETHOD(ScriptStopwatch, Elapsed),
					asCALL_THISCALL) >= 0 &&
				engine.RegisterObjectMethod(
					"Stopwatch",
					"int64 get_ElapsedMilliseconds() const property",
					asMETHOD(ScriptStopwatch, ElapsedMilliseconds),
					asCALL_THISCALL) >= 0 &&
				engine.RegisterObjectMethod(
					"Stopwatch",
					"double get_ElapsedSeconds() const property",
					asMETHOD(ScriptStopwatch, ElapsedSeconds),
					asCALL_THISCALL) >= 0;
			if (!typeRegistered)
				return false;

			engine.SetDefaultNamespace("Stopwatch");
			const bool functionsRegistered =
				engine.RegisterGlobalFunction(
					"Stopwatch StartNew()",
					asFUNCTION(StartNewStopwatch), asCALL_CDECL) >= 0;
			engine.SetDefaultNamespace("");
			return functionsRegistered;
		}

		bool RegisterGuid(asIScriptEngine& engine)
		{
			const bool typeRegistered =
				engine.RegisterObjectType(
					"Guid", sizeof(ScriptGuid),
					asOBJ_VALUE |
						asGetTypeTraits<ScriptGuid>() |
						asOBJ_APP_CLASS_ALLINTS) >= 0 &&
				engine.RegisterObjectBehaviour(
					"Guid", asBEHAVE_CONSTRUCT, "void f()",
					asFUNCTION(DefaultConstruct<ScriptGuid>),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectBehaviour(
					"Guid", asBEHAVE_CONSTRUCT,
					"void f(const Guid &in)",
					asFUNCTION(CopyConstruct<ScriptGuid>),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectBehaviour(
					"Guid", asBEHAVE_DESTRUCT, "void f()",
					asFUNCTION(Destruct<ScriptGuid>),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"Guid", "Guid &opAssign(const Guid &in)",
					asFUNCTION(Assign<ScriptGuid>),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"Guid", "bool opEquals(const Guid &in other) const",
					asMETHOD(ScriptGuid, Equals),
					asCALL_THISCALL) >= 0 &&
				engine.RegisterObjectMethod(
					"Guid", "bool get_IsEmpty() const property",
					asMETHOD(ScriptGuid, IsEmpty),
					asCALL_THISCALL) >= 0 &&
				engine.RegisterObjectMethod(
					"Guid", "string ToString() const",
					asMETHOD(ScriptGuid, ToString),
					asCALL_THISCALL) >= 0;
			if (!typeRegistered)
				return false;

			engine.SetDefaultNamespace("Guid");
			const bool functionsRegistered =
				engine.RegisterGlobalProperty(
					"const Guid Empty",
					const_cast<ScriptGuid*>(&GuidEmpty)) >= 0 &&
				engine.RegisterGlobalFunction(
					"Guid NewGuid()",
					asFUNCTION(NewGuid), asCALL_CDECL) >= 0 &&
				engine.RegisterGlobalFunction(
					"Guid Parse(const string &in value)",
					asFUNCTION(ParseGuid), asCALL_CDECL) >= 0 &&
				engine.RegisterGlobalFunction(
					"bool TryParse(const string &in value, Guid &out result)",
					asFUNCTION(TryParseGuid), asCALL_CDECL) >= 0;
			engine.SetDefaultNamespace("");
			return functionsRegistered;
		}

		bool RegisterStringHelpers(asIScriptEngine& engine)
		{
			const bool methodsRegistered =
				engine.RegisterObjectMethod(
					"string",
					"bool Contains(const string &in value) const",
					asFUNCTION(StringContains),
					asCALL_CDECL_OBJFIRST) >= 0 &&
				engine.RegisterObjectMethod(
					"string",
					"bool StartsWith(const string &in value) const",
					asFUNCTION(StringStartsWith),
					asCALL_CDECL_OBJFIRST) >= 0 &&
				engine.RegisterObjectMethod(
					"string",
					"bool EndsWith(const string &in value) const",
					asFUNCTION(StringEndsWith),
					asCALL_CDECL_OBJFIRST) >= 0 &&
				engine.RegisterObjectMethod(
					"string", "string ToUpper() const",
					asFUNCTION(StringToUpper),
					asCALL_CDECL_OBJFIRST) >= 0 &&
				engine.RegisterObjectMethod(
					"string", "string ToLower() const",
					asFUNCTION(StringToLower),
					asCALL_CDECL_OBJFIRST) >= 0 &&
				engine.RegisterObjectMethod(
					"string", "string Trim() const",
					asFUNCTION(StringTrim),
					asCALL_CDECL_OBJFIRST) >= 0 &&
				engine.RegisterObjectMethod(
					"string",
					"string Replace(const string &in oldValue, "
						"const string &in newValue) const",
					asFUNCTION(StringReplace),
					asCALL_CDECL_OBJFIRST) >= 0 &&
				engine.RegisterObjectMethod(
					"string", "uint get_Length() const property",
					asFUNCTION(StringLength),
					asCALL_CDECL_OBJFIRST) >= 0;
			if (!methodsRegistered)
				return false;

			engine.SetDefaultNamespace("String");
			const bool functionsRegistered =
				engine.RegisterGlobalFunction(
					"bool IsNullOrEmpty(const string &in value)",
					asFUNCTION(IsNullOrEmpty), asCALL_CDECL) >= 0 &&
				engine.RegisterGlobalFunction(
					"bool IsNullOrWhiteSpace(const string &in value)",
					asFUNCTION(IsNullOrWhiteSpace), asCALL_CDECL) >= 0 &&
				engine.RegisterGlobalFunction(
					"int CompareOrdinal(const string &in left, "
						"const string &in right)",
					asFUNCTION(CompareOrdinal), asCALL_CDECL) >= 0;
			engine.SetDefaultNamespace("");
			return functionsRegistered;
		}

		bool RegisterPath(asIScriptEngine& engine)
		{
			engine.SetDefaultNamespace("Path");
			const bool registered =
				engine.RegisterGlobalFunction(
					"string Combine(const string &in path1, "
						"const string &in path2)",
					asFUNCTION(PathCombine), asCALL_CDECL) >= 0 &&
				engine.RegisterGlobalFunction(
					"string GetFileName(const string &in path)",
					asFUNCTION(PathGetFileName), asCALL_CDECL) >= 0 &&
				engine.RegisterGlobalFunction(
					"string GetFileNameWithoutExtension("
						"const string &in path)",
					asFUNCTION(PathGetFileNameWithoutExtension),
					asCALL_CDECL) >= 0 &&
				engine.RegisterGlobalFunction(
					"string GetExtension(const string &in path)",
					asFUNCTION(PathGetExtension), asCALL_CDECL) >= 0 &&
				engine.RegisterGlobalFunction(
					"string GetDirectoryName(const string &in path)",
					asFUNCTION(PathGetDirectoryName), asCALL_CDECL) >= 0 &&
				engine.RegisterGlobalFunction(
					"string ChangeExtension(const string &in path, "
						"const string &in extension)",
					asFUNCTION(PathChangeExtension), asCALL_CDECL) >= 0 &&
				engine.RegisterGlobalFunction(
					"bool HasExtension(const string &in path)",
					asFUNCTION(PathHasExtension), asCALL_CDECL) >= 0;
			engine.SetDefaultNamespace("");
			return registered;
		}
	}

	bool RegisterCoreHelpersApi(
		asIScriptEngine& engine,
		std::string& error)
	{
		if (!RegisterRandom(engine))
			error = "Could not register the Random API.";
		else if (!RegisterStringBuilder(engine))
			error = "Could not register the StringBuilder API.";
		else if (!RegisterTimeSpan(engine))
			error = "Could not register the TimeSpan API.";
		else if (!RegisterStopwatch(engine))
			error = "Could not register the Stopwatch API.";
		else if (!RegisterGuid(engine))
			error = "Could not register the Guid API.";
		else if (!RegisterStringHelpers(engine))
			error = "Could not register the String helper API.";
		else if (!RegisterPath(engine))
			error = "Could not register the Path API.";
		else
			return true;

		return false;
	}
}
