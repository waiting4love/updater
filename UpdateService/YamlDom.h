#pragma once

#include <vector>
#include <unordered_map>
#include <string>
#include <string_view>
#include <functional>
#include <stdexcept>
#include <optional>

namespace yaml {

	class Value;
	using Sequence = std::vector<Value>;
	class Mapping;
	class ParserContext;

	template<class T>
	class RefOptional {
	private:
		std::optional<std::reference_wrapper<const T>> opt;
	public:
		RefOptional() = default;
		RefOptional(const T& v) :opt(v) {}
		bool has_value() const { return opt.has_value(); }
		const T& value() const { return opt.value().get(); }
		const T& value_or(const T& default_value) const { return opt.value_or(default_value).get(); }
	};

	using ValueRef = RefOptional<Value>;
	using SequenceRef = RefOptional<Sequence>;
	using MappingRef = RefOptional<Mapping>;
	using StringRef = RefOptional<std::string>;

	class Mapping {
	public:
		bool hasKey(const std::string& name) const;

		using KeyValue = std::unordered_map<std::string, Value>;
		const KeyValue& map() const { return map_; }

		ValueRef getValue(const std::string& name) const;
		MappingRef getMapping(const std::string& name) const;
		SequenceRef getSequence(const std::string& name) const;
		StringRef getString(const std::string& name) const;
		const std::string& getString(const std::string& name, const std::string& default_value) const;

		Mapping() = default;
		Mapping(Mapping&&) = default;
		Mapping(const Mapping&) = delete;

		Mapping& operator=(const Mapping&) = delete;

	private:
		friend class ParserContext;

		KeyValue map_;
	};

	class Value {
	public:
		MappingRef getMapping() const {
			if (type_ != Type::Mapping) return MappingRef{};
			return mapping_;
		}

		SequenceRef getSequence() const {
			if (type_ != Type::Sequence) return SequenceRef{};
			return sequence_;
		}

		StringRef getString() const {
			if (type_ != Type::String) return StringRef{};
			return string_;
		}

		bool isString() const { return type_ == Type::String; }
		bool isMapping() const { return type_ == Type::Mapping; }
		bool isSequence() const { return type_ == Type::Sequence; }

		explicit Value(std::string&& s) : type_(Type::String), string_(std::move(s)) {}
		explicit Value(Mapping&& mapping) : type_(Type::Mapping), mapping_(std::move(mapping)) {}
		explicit Value(Sequence&& sequence) : type_(Type::Sequence), sequence_(std::move(sequence)) {}

		static const Value& getEmpty() {
			static Value empty{ "" };
			return empty;
		}

	private:
		friend class ParserContext;

		enum class Type {
			String, Mapping, Sequence
		};

		Type type_;
		std::string string_;
		Mapping mapping_;
		Sequence sequence_;
	};

	class parse_format_error : public std::logic_error
	{
	public:
		using std::logic_error::logic_error;
	};

	class parse_unexpect_error : public parse_format_error
	{
	public:
		using parse_format_error::parse_format_error;
	};

	Value parseFile(const char* filename);
	Value parseString(std::string_view s);

} // namespace yaml