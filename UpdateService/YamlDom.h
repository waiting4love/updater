#pragma once

#include <vector>
#include <unordered_map>
#include <string>
#include <string_view>
#include <functional>
#include <stdexcept>

namespace yaml {

	class Value;
	using Sequence = std::vector<Value>;

	class ParserContext;

	class Mapping {
	public:
		bool hasKey(const std::string& name) const;

		using KeyValue = std::unordered_map<std::string, Value>;
		const KeyValue& map() const { return map_; }

		const Value& getValue(const std::string& name) const;
		const Mapping& getMapping(const std::string& name) const;
		const Sequence& getSequence(const std::string& name) const;
		const std::string& getString(const std::string& name) const;
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
		const Mapping& getMapping() const {
			if (type_ != Type::Mapping)
				throw std::out_of_range("Value is not of Mapping type");
			return std::cref(mapping_);
		}

		const Sequence& getSequence() const {
			if (type_ != Type::Sequence)
				throw std::out_of_range("Value is not of Sequence type");
			return std::cref(sequence_);
		}

		const std::string& getString() const {
			if (type_ != Type::String)
				throw std::out_of_range("Value is not of String type");
			return std::cref(string_);
		}

		bool isString() const { return type_ == Type::String; }
		bool isMapping() const { return type_ == Type::Mapping; }
		bool isSequence() const { return type_ == Type::Sequence; }

		Value(std::string&& s) : type_(Type::String), string_(std::move(s)) {}
		Value(Mapping&& mapping) : type_(Type::Mapping), mapping_(std::move(mapping)) {}
		Value(Sequence&& sequence) : type_(Type::Sequence), sequence_(std::move(sequence)) {}

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