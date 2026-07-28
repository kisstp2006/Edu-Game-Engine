#ifndef EGE_TYPE_REGISTRY_H
#define EGE_TYPE_REGISTRY_H

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <shared_mutex>
#include <string>
#include <type_traits>
#include <typeindex>
#include <unordered_map>
#include <variant>
#include <vector>

namespace EGE
{
	enum class PropertyKind
	{
		Unsupported,
		Boolean,
		Int8,
		Int16,
		Int32,
		Int64,
		UInt8,
		UInt16,
		UInt32,
		UInt64,
		Float,
		Double,
		String,
		Enumeration,
		GameObjectReference,
		ComponentReference,
		Vector3,
		Color
	};

	struct Vector3Value
	{
		float x = 0.0f;
		float y = 0.0f;
		float z = 0.0f;

		bool operator==(const Vector3Value&) const = default;
	};

	struct ColorValue
	{
		float r = 0.0f;
		float g = 0.0f;
		float b = 0.0f;
		float a = 1.0f;

		bool operator==(const ColorValue&) const = default;
	};

	struct GameObjectReferenceValue
	{
		std::uint64_t objectId = 0;

		bool operator==(const GameObjectReferenceValue&) const = default;
	};

	struct ComponentReferenceValue
	{
		std::uint64_t objectId = 0;
		std::uint64_t componentId = 0;

		bool operator==(const ComponentReferenceValue&) const = default;
	};

	using PropertyValue = std::variant<
		std::monostate,
		bool,
		std::int64_t,
		std::uint64_t,
		double,
		std::string,
		GameObjectReferenceValue,
		ComponentReferenceValue,
		Vector3Value,
		ColorValue>;

	struct PropertyRange
	{
		double minimum = 0.0;
		double maximum = 0.0;
	};

	struct PropertyAttributes
	{
		bool serialized = true;
		bool visible = true;
		bool readOnly = false;
		std::string header;
		std::string tooltip;
		std::optional<PropertyRange> range;
	};

	struct PropertyEnumValue
	{
		std::string name;
		std::string displayName;
		std::int64_t value = 0;
	};

	struct PropertyDescriptor
	{
		using Reader = std::function<bool(const void*, PropertyValue&)>;
		using Writer = std::function<bool(void*, const PropertyValue&)>;

		std::string name;
		std::string displayName;
		std::string typeName;
		PropertyKind kind = PropertyKind::Unsupported;
		PropertyAttributes attributes;
		std::vector<PropertyEnumValue> enumValues;
		Reader reader;
		Writer writer;

		[[nodiscard]] bool Read(
			const void* object, PropertyValue& value) const;
		bool Write(void* object, PropertyValue value) const;
	};

	struct TypeDescriptor
	{
		std::string domain;
		std::string id;
		std::string displayName;
		std::type_index nativeType = typeid(void);
		std::vector<PropertyDescriptor> properties;

		[[nodiscard]] const PropertyDescriptor* FindProperty(
			const std::string& name) const;
	};

	class TypeRegistry final
	{
	public:
		static TypeRegistry& Get();

		bool Register(TypeDescriptor descriptor, std::string& error);
		bool ReplaceDomain(
			const std::string& domain,
			std::vector<TypeDescriptor> descriptors,
			std::string& error);
		[[nodiscard]] bool ValidateDomain(
			const std::string& domain,
			const std::vector<TypeDescriptor>& descriptors,
			std::string& error) const;
		void ClearDomain(const std::string& domain);

		[[nodiscard]] std::shared_ptr<const TypeDescriptor> Find(
			const std::string& domain, const std::string& id) const;
		[[nodiscard]] std::shared_ptr<const TypeDescriptor> Find(
			std::type_index nativeType) const;
		[[nodiscard]] std::vector<std::shared_ptr<const TypeDescriptor>>
			GetDomain(const std::string& domain) const;

	private:
		using TypeMap = std::unordered_map<
			std::string, std::shared_ptr<const TypeDescriptor>>;

		static std::string MakeKey(
			const std::string& domain, const std::string& id);
		static bool Validate(
			const TypeDescriptor& descriptor, std::string& error);

		mutable std::shared_mutex mutex_;
		TypeMap types_;
		std::unordered_map<
			std::type_index, std::shared_ptr<const TypeDescriptor>>
			nativeTypes_;
	};

	struct PropertyState
	{
		std::string name;
		PropertyKind kind = PropertyKind::Unsupported;
		PropertyValue value;
	};

	using PropertyBag = std::vector<PropertyState>;

	[[nodiscard]] std::string PropertyKindName(PropertyKind kind);
	bool ParsePropertyKind(const std::string& text, PropertyKind& kind);
	[[nodiscard]] std::string HumanizeIdentifier(const std::string& name);
	bool CoercePropertyValue(
		PropertyKind targetKind,
		const PropertyValue& source,
		PropertyValue& result);

	template<typename Value>
	constexpr PropertyKind PropertyKindOf()
	{
		using Type = std::remove_cv_t<Value>;
		if constexpr (std::is_same_v<Type, bool>)
			return PropertyKind::Boolean;
		else if constexpr (std::is_same_v<Type, std::int8_t>)
			return PropertyKind::Int8;
		else if constexpr (std::is_same_v<Type, std::int16_t>)
			return PropertyKind::Int16;
		else if constexpr (std::is_same_v<Type, std::int32_t>)
			return PropertyKind::Int32;
		else if constexpr (std::is_same_v<Type, std::int64_t>)
			return PropertyKind::Int64;
		else if constexpr (std::is_same_v<Type, std::uint8_t>)
			return PropertyKind::UInt8;
		else if constexpr (std::is_same_v<Type, std::uint16_t>)
			return PropertyKind::UInt16;
		else if constexpr (std::is_same_v<Type, std::uint32_t>)
			return PropertyKind::UInt32;
		else if constexpr (std::is_same_v<Type, std::uint64_t>)
			return PropertyKind::UInt64;
		else if constexpr (std::is_same_v<Type, float>)
			return PropertyKind::Float;
		else if constexpr (std::is_same_v<Type, double>)
			return PropertyKind::Double;
		else if constexpr (std::is_same_v<Type, std::string>)
			return PropertyKind::String;
		else if constexpr (std::is_same_v<Type, Vector3Value>)
			return PropertyKind::Vector3;
		else if constexpr (std::is_same_v<Type, ColorValue>)
			return PropertyKind::Color;
		else if constexpr (std::is_enum_v<Type>)
			return PropertyKind::Enumeration;
		else
			return PropertyKind::Unsupported;
	}

	template<typename Object, typename Value>
	PropertyDescriptor MakeMemberProperty(
		std::string name,
		Value Object::* member,
		PropertyAttributes attributes = {})
	{
		PropertyDescriptor descriptor;
		descriptor.name = std::move(name);
		descriptor.displayName = HumanizeIdentifier(descriptor.name);
		descriptor.kind = PropertyKindOf<Value>();
		descriptor.attributes = std::move(attributes);
		descriptor.reader = [member](
			const void* object, PropertyValue& value)
		{
			if (!object)
				return false;

			const Value& source =
				static_cast<const Object*>(object)->*member;
			if constexpr (std::is_same_v<Value, bool>)
				value = source;
			else if constexpr (
				std::is_integral_v<Value> && std::is_signed_v<Value>)
				value = static_cast<std::int64_t>(source);
			else if constexpr (
				std::is_integral_v<Value> && std::is_unsigned_v<Value>)
				value = static_cast<std::uint64_t>(source);
			else if constexpr (std::is_enum_v<Value>)
				value = static_cast<std::int64_t>(source);
			else if constexpr (std::is_floating_point_v<Value>)
				value = static_cast<double>(source);
			else if constexpr (std::is_same_v<Value, std::string>)
				value = source;
			else if constexpr (
				std::is_same_v<Value, Vector3Value> ||
				std::is_same_v<Value, ColorValue>)
			{
				value = source;
			}
			else
				return false;
			return true;
		};
		descriptor.writer = [member](
			void* object, const PropertyValue& value)
		{
			if (!object)
				return false;

			PropertyValue converted;
			if (!CoercePropertyValue(
					PropertyKindOf<Value>(), value, converted))
				return false;

			Value& destination = static_cast<Object*>(object)->*member;
			if constexpr (std::is_same_v<Value, bool>)
				destination = std::get<bool>(converted);
			else if constexpr (
				std::is_integral_v<Value> && std::is_signed_v<Value>)
				destination =
					static_cast<Value>(std::get<std::int64_t>(converted));
			else if constexpr (
				std::is_integral_v<Value> && std::is_unsigned_v<Value>)
				destination =
					static_cast<Value>(std::get<std::uint64_t>(converted));
			else if constexpr (std::is_enum_v<Value>)
				destination =
					static_cast<Value>(std::get<std::int64_t>(converted));
			else if constexpr (std::is_floating_point_v<Value>)
				destination =
					static_cast<Value>(std::get<double>(converted));
			else if constexpr (std::is_same_v<Value, std::string>)
				destination = std::get<std::string>(converted);
			else if constexpr (std::is_same_v<Value, Vector3Value>)
				destination = std::get<Vector3Value>(converted);
			else if constexpr (std::is_same_v<Value, ColorValue>)
				destination = std::get<ColorValue>(converted);
			else
				return false;
			return true;
		};
		return descriptor;
	}
}

#endif
