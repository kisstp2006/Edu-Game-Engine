#include "TypeRegistry.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <limits>
#include <mutex>
#include <unordered_set>

namespace EGE
{
	namespace
	{
		bool IsSignedInteger(PropertyKind kind)
		{
			return kind == PropertyKind::Int8 ||
				kind == PropertyKind::Int16 ||
				kind == PropertyKind::Int32 ||
				kind == PropertyKind::Int64 ||
				kind == PropertyKind::Enumeration;
		}

		bool IsUnsignedInteger(PropertyKind kind)
		{
			return kind == PropertyKind::UInt8 ||
				kind == PropertyKind::UInt16 ||
				kind == PropertyKind::UInt32 ||
				kind == PropertyKind::UInt64;
		}

		bool IsFloatingPoint(PropertyKind kind)
		{
			return kind == PropertyKind::Float ||
				kind == PropertyKind::Double;
		}

		std::int64_t SignedMinimum(PropertyKind kind)
		{
			switch (kind)
			{
				case PropertyKind::Int8:
					return std::numeric_limits<std::int8_t>::min();
				case PropertyKind::Int16:
					return std::numeric_limits<std::int16_t>::min();
				case PropertyKind::Int32:
				case PropertyKind::Enumeration:
					return std::numeric_limits<std::int32_t>::min();
				default:
					return std::numeric_limits<std::int64_t>::min();
			}
		}

		std::int64_t SignedMaximum(PropertyKind kind)
		{
			switch (kind)
			{
				case PropertyKind::Int8:
					return std::numeric_limits<std::int8_t>::max();
				case PropertyKind::Int16:
					return std::numeric_limits<std::int16_t>::max();
				case PropertyKind::Int32:
				case PropertyKind::Enumeration:
					return std::numeric_limits<std::int32_t>::max();
				default:
					return std::numeric_limits<std::int64_t>::max();
			}
		}

		std::uint64_t UnsignedMaximum(PropertyKind kind)
		{
			switch (kind)
			{
				case PropertyKind::UInt8:
					return std::numeric_limits<std::uint8_t>::max();
				case PropertyKind::UInt16:
					return std::numeric_limits<std::uint16_t>::max();
				case PropertyKind::UInt32:
					return std::numeric_limits<std::uint32_t>::max();
				default:
					return std::numeric_limits<std::uint64_t>::max();
			}
		}

		double ToDouble(const PropertyValue& value, bool& valid)
		{
			valid = true;
			if (const auto* number = std::get_if<double>(&value))
				return *number;
			if (const auto* number = std::get_if<std::int64_t>(&value))
				return static_cast<double>(*number);
			if (const auto* number = std::get_if<std::uint64_t>(&value))
				return static_cast<double>(*number);
			valid = false;
			return 0.0;
		}

		std::int64_t BoundedSigned(double value)
		{
			if (value <= static_cast<double>(
					std::numeric_limits<std::int64_t>::min()))
			{
				return std::numeric_limits<std::int64_t>::min();
			}
			if (value >= static_cast<double>(
					std::numeric_limits<std::int64_t>::max()))
			{
				return std::numeric_limits<std::int64_t>::max();
			}
			return static_cast<std::int64_t>(value);
		}

		std::uint64_t BoundedUnsigned(double value)
		{
			if (value <= 0.0)
				return 0;
			if (value >= static_cast<double>(
					std::numeric_limits<std::uint64_t>::max()))
			{
				return std::numeric_limits<std::uint64_t>::max();
			}
			return static_cast<std::uint64_t>(value);
		}
	}

	bool PropertyDescriptor::Read(
		const void* object, PropertyValue& value) const
	{
		return reader && reader(object, value);
	}

	bool PropertyDescriptor::Write(
		void* object, PropertyValue value) const
	{
		if (!writer || attributes.readOnly)
			return false;

		PropertyValue converted;
		if (!CoercePropertyValue(kind, value, converted))
			return false;

		if (attributes.range)
		{
			const PropertyRange& range = *attributes.range;
			if (auto* number = std::get_if<std::int64_t>(&converted))
			{
				const std::int64_t minimum =
					BoundedSigned(std::ceil(range.minimum));
				const std::int64_t maximum =
					BoundedSigned(std::floor(range.maximum));
				if (minimum > maximum)
					return false;
				*number = std::clamp(
					*number, minimum, maximum);
			}
			else if (auto* number =
				std::get_if<std::uint64_t>(&converted))
			{
				const double minimum = std::max(0.0, range.minimum);
				const double maximum = std::max(minimum, range.maximum);
				const std::uint64_t minimumValue =
					BoundedUnsigned(std::ceil(minimum));
				const std::uint64_t maximumValue =
					BoundedUnsigned(std::floor(maximum));
				if (minimumValue > maximumValue)
					return false;
				*number = std::clamp(
					*number, minimumValue, maximumValue);
			}
			else if (auto* number = std::get_if<double>(&converted))
			{
				*number = std::clamp(
					*number, range.minimum, range.maximum);
			}
		}

		return writer(object, converted);
	}

	const PropertyDescriptor* TypeDescriptor::FindProperty(
		const std::string& name) const
	{
		const auto iterator = std::find_if(
			properties.begin(),
			properties.end(),
			[&name](const PropertyDescriptor& property)
			{
				return property.name == name;
			});
		return iterator == properties.end() ? nullptr : &*iterator;
	}

	TypeRegistry& TypeRegistry::Get()
	{
		static TypeRegistry registry;
		return registry;
	}

	bool TypeRegistry::Register(
		TypeDescriptor descriptor, std::string& error)
	{
		if (!Validate(descriptor, error))
			return false;

		auto stored =
			std::make_shared<const TypeDescriptor>(std::move(descriptor));
		std::unique_lock lock(mutex_);
		const std::string key = MakeKey(stored->domain, stored->id);
		const auto existing = types_.find(key);
		if (existing != types_.end() &&
			existing->second->nativeType != typeid(void))
		{
			const auto native =
				nativeTypes_.find(existing->second->nativeType);
			if (native != nativeTypes_.end() &&
				native->second == existing->second)
			{
				nativeTypes_.erase(native);
			}
		}
		types_[key] = stored;
		if (stored->nativeType != typeid(void))
			nativeTypes_[stored->nativeType] = stored;
		return true;
	}

	bool TypeRegistry::ReplaceDomain(
		const std::string& domain,
		std::vector<TypeDescriptor> descriptors,
		std::string& error)
	{
		if (domain.empty())
		{
			error = "The reflection domain is empty.";
			return false;
		}

		TypeMap prepared;
		std::unordered_map<
			std::type_index, std::shared_ptr<const TypeDescriptor>>
			preparedNative;
		for (TypeDescriptor& descriptor : descriptors)
		{
			descriptor.domain = domain;
			if (!Validate(descriptor, error))
				return false;

			auto stored = std::make_shared<const TypeDescriptor>(
				std::move(descriptor));
			const std::string key = MakeKey(domain, stored->id);
			if (!prepared.emplace(key, stored).second)
			{
				error = "Duplicate reflected type '" + stored->id + "'.";
				return false;
			}
			if (stored->nativeType != typeid(void))
				preparedNative[stored->nativeType] = stored;
		}

		std::unique_lock lock(mutex_);
		for (auto iterator = types_.begin(); iterator != types_.end();)
		{
			if (iterator->second->domain == domain)
				iterator = types_.erase(iterator);
			else
				++iterator;
		}
		for (auto iterator = nativeTypes_.begin();
			iterator != nativeTypes_.end();)
		{
			if (iterator->second->domain == domain)
				iterator = nativeTypes_.erase(iterator);
			else
				++iterator;
		}
		types_.insert(prepared.begin(), prepared.end());
		nativeTypes_.insert(preparedNative.begin(), preparedNative.end());
		return true;
	}

	bool TypeRegistry::ValidateDomain(
		const std::string& domain,
		const std::vector<TypeDescriptor>& descriptors,
		std::string& error) const
	{
		if (domain.empty())
		{
			error = "The reflection domain is empty.";
			return false;
		}

		std::unordered_set<std::string> ids;
		for (TypeDescriptor descriptor : descriptors)
		{
			descriptor.domain = domain;
			if (!Validate(descriptor, error))
				return false;
			if (!ids.insert(descriptor.id).second)
			{
				error = "Duplicate reflected type '" + descriptor.id + "'.";
				return false;
			}
		}
		return true;
	}

	void TypeRegistry::ClearDomain(const std::string& domain)
	{
		std::unique_lock lock(mutex_);
		for (auto iterator = types_.begin(); iterator != types_.end();)
		{
			if (iterator->second->domain == domain)
				iterator = types_.erase(iterator);
			else
				++iterator;
		}
		for (auto iterator = nativeTypes_.begin();
			iterator != nativeTypes_.end();)
		{
			if (iterator->second->domain == domain)
				iterator = nativeTypes_.erase(iterator);
			else
				++iterator;
		}
	}

	std::shared_ptr<const TypeDescriptor> TypeRegistry::Find(
		const std::string& domain, const std::string& id) const
	{
		std::shared_lock lock(mutex_);
		const auto iterator = types_.find(MakeKey(domain, id));
		return iterator == types_.end() ? nullptr : iterator->second;
	}

	std::shared_ptr<const TypeDescriptor> TypeRegistry::Find(
		std::type_index nativeType) const
	{
		std::shared_lock lock(mutex_);
		const auto iterator = nativeTypes_.find(nativeType);
		return iterator == nativeTypes_.end() ? nullptr : iterator->second;
	}

	std::vector<std::shared_ptr<const TypeDescriptor>>
		TypeRegistry::GetDomain(const std::string& domain) const
	{
		std::vector<std::shared_ptr<const TypeDescriptor>> result;
		std::shared_lock lock(mutex_);
		for (const auto& [key, descriptor] : types_)
		{
			(void)key;
			if (descriptor->domain == domain)
				result.push_back(descriptor);
		}
		std::sort(
			result.begin(),
			result.end(),
			[](const auto& left, const auto& right)
			{
				return left->displayName < right->displayName;
			});
		return result;
	}

	std::string TypeRegistry::MakeKey(
		const std::string& domain, const std::string& id)
	{
		return domain + '\x1f' + id;
	}

	bool TypeRegistry::Validate(
		const TypeDescriptor& descriptor, std::string& error)
	{
		if (descriptor.domain.empty() || descriptor.id.empty())
		{
			error = "A reflected type needs a domain and an id.";
			return false;
		}

		std::unordered_set<std::string> names;
		for (const PropertyDescriptor& property : descriptor.properties)
		{
			if (property.name.empty())
			{
				error = "A reflected property has an empty name.";
				return false;
			}
			if (!names.insert(property.name).second)
			{
				error = "Duplicate reflected property '" +
					property.name + "' in type '" + descriptor.id + "'.";
				return false;
			}
			if (property.kind != PropertyKind::Unsupported &&
				(!property.reader ||
				 (!property.attributes.readOnly && !property.writer)))
			{
				error = "Reflected property '" + property.name +
					"' has no accessor.";
				return false;
			}
			if (property.attributes.range &&
				(!std::isfinite(property.attributes.range->minimum) ||
				 !std::isfinite(property.attributes.range->maximum) ||
				 property.attributes.range->minimum >
					property.attributes.range->maximum))
			{
				error = "Reflected property '" + property.name +
					"' has an invalid range.";
				return false;
			}
		}
		return true;
	}

	std::string PropertyKindName(PropertyKind kind)
	{
		switch (kind)
		{
			case PropertyKind::Boolean: return "Boolean";
			case PropertyKind::Int8: return "Int8";
			case PropertyKind::Int16: return "Int16";
			case PropertyKind::Int32: return "Int32";
			case PropertyKind::Int64: return "Int64";
			case PropertyKind::UInt8: return "UInt8";
			case PropertyKind::UInt16: return "UInt16";
			case PropertyKind::UInt32: return "UInt32";
			case PropertyKind::UInt64: return "UInt64";
			case PropertyKind::Float: return "Float";
			case PropertyKind::Double: return "Double";
			case PropertyKind::String: return "String";
			case PropertyKind::Enumeration: return "Enumeration";
			case PropertyKind::GameObjectReference:
				return "GameObjectReference";
			case PropertyKind::ComponentReference:
				return "ComponentReference";
			default: return "Unsupported";
		}
	}

	bool ParsePropertyKind(const std::string& text, PropertyKind& kind)
	{
		for (int value = static_cast<int>(PropertyKind::Unsupported);
			value <= static_cast<int>(PropertyKind::ComponentReference);
			++value)
		{
			const auto candidate = static_cast<PropertyKind>(value);
			if (PropertyKindName(candidate) == text)
			{
				kind = candidate;
				return true;
			}
		}
		return false;
	}

	std::string HumanizeIdentifier(const std::string& name)
	{
		std::string result;
		result.reserve(name.size() + 4);
		for (std::size_t index = 0; index < name.size(); ++index)
		{
			const char character = name[index];
			if (character == '_')
			{
				if (!result.empty() && result.back() != ' ')
					result.push_back(' ');
				continue;
			}
			if (index > 0 &&
				std::isupper(static_cast<unsigned char>(character)) &&
				!std::isupper(static_cast<unsigned char>(name[index - 1])))
			{
				result.push_back(' ');
			}
			result.push_back(character);
		}
		if (!result.empty())
			result.front() = static_cast<char>(
				std::toupper(static_cast<unsigned char>(result.front())));
		return result;
	}

	bool CoercePropertyValue(
		PropertyKind targetKind,
		const PropertyValue& source,
		PropertyValue& result)
	{
		if (targetKind == PropertyKind::Boolean)
		{
			if (const auto* value = std::get_if<bool>(&source))
			{
				result = *value;
				return true;
			}
			return false;
		}

		if (targetKind == PropertyKind::String)
		{
			if (const auto* value = std::get_if<std::string>(&source))
			{
				result = *value;
				return true;
			}
			return false;
		}

		if (targetKind == PropertyKind::GameObjectReference)
		{
			if (const auto* value =
				std::get_if<GameObjectReferenceValue>(&source))
			{
				result = *value;
				return true;
			}
			return false;
		}

		if (targetKind == PropertyKind::ComponentReference)
		{
			if (const auto* value =
				std::get_if<ComponentReferenceValue>(&source))
			{
				result = *value;
				return true;
			}
			return false;
		}

		if (IsSignedInteger(targetKind))
		{
			std::int64_t value = 0;
			if (const auto* signedValue =
				std::get_if<std::int64_t>(&source))
			{
				value = *signedValue;
			}
			else if (const auto* unsignedValue =
				std::get_if<std::uint64_t>(&source))
			{
				value = *unsignedValue >
					static_cast<std::uint64_t>(
						std::numeric_limits<std::int64_t>::max())
					? std::numeric_limits<std::int64_t>::max()
					: static_cast<std::int64_t>(*unsignedValue);
			}
			else
			{
				bool valid = false;
				const double number = ToDouble(source, valid);
				if (!valid || !std::isfinite(number))
					return false;
				value = BoundedSigned(number);
			}
			result = std::clamp(
				value, SignedMinimum(targetKind), SignedMaximum(targetKind));
			return true;
		}

		if (IsUnsignedInteger(targetKind))
		{
			std::uint64_t value = 0;
			if (const auto* unsignedValue =
				std::get_if<std::uint64_t>(&source))
			{
				value = *unsignedValue;
			}
			else if (const auto* signedValue =
				std::get_if<std::int64_t>(&source))
			{
				value = *signedValue < 0
					? 0
					: static_cast<std::uint64_t>(*signedValue);
			}
			else
			{
				bool valid = false;
				const double number = ToDouble(source, valid);
				if (!valid || !std::isfinite(number))
					return false;
				value = BoundedUnsigned(number);
			}
			result = std::min(value, UnsignedMaximum(targetKind));
			return true;
		}

		if (IsFloatingPoint(targetKind))
		{
			bool valid = false;
			double value = ToDouble(source, valid);
			if (!valid || !std::isfinite(value))
				return false;
			if (targetKind == PropertyKind::Float)
			{
				value = std::clamp(
					value,
					-static_cast<double>(
						std::numeric_limits<float>::max()),
					static_cast<double>(
						std::numeric_limits<float>::max()));
			}
			result = value;
			return true;
		}

		return false;
	}
}
