#pragma once

#include <string>
#include <map>
#include <vector>
#include <deque>
#include <functional>
#include <sstream>
#include <cctype>

namespace http_parser {

	template <typename ItrType>
	class header_tokenizer {

	public:

		template <typename CharType>
		static bool is_space(CharType ch)
		{
			return ch == ' '
				|| ch == '\t'
				|| ch == '\r'
				;
		};

		template <typename CharType>
		static bool is_eol(CharType ch)
		{
			return ch == '\n'
				;
		};

		template <typename CharType>
		static bool is_special(CharType val)
		{
			return val == '\n'
				|| val == ':'
				;
		}

		template <typename Itr>
		static Itr skip_spaces(Itr b, Itr e)
		{
			while ((b != e) && is_space(*b)) {
				++b;
			}
			return b;
		}

		template <typename Itr>
		static Itr skip_non_eol(Itr b, Itr e)
		{
			while ((b != e) && !is_eol(*b)) {
				++b;
			}
			return b;
		}

		template <typename Itr>
		static Itr skip_non_spaces(Itr b, Itr e)
		{
			while ((b != e) && !is_space(*b)) {
				++b;
			}
			return b;
		}

		template <typename Itr>
		static Itr skip_non_spaces_spacial(Itr b, Itr e)
		{
			while ((b != e) && !is_space(*b) && !is_special(*b)) {
				++b;
			}
			return b;
		}
	};

	class flow_collector {

	public:

		using parser_call = std::function<bool()>;
		using container_type = std::deque<char>;
		using tokenizer = header_tokenizer<container_type::const_iterator>;
		using header_map = std::multimap<std::string, std::string>;

		flow_collector()
		{
			current_parser_ = [this]() { return parse_first(); };
		}

		template <typename Itr>
		std::size_t feed(Itr begin, Itr end)
		{
			full_data_.insert(full_data_.end(), begin, end);
			while (current_parser_()) {}
			return full_data_.size();
		}

		void finalize()
		{
			finalized_ = true;
			process_end();
		}

		std::size_t expected_size() const
		{
			return expected_length_;
		}

		virtual bool body_process()
		{
			return false;
		}

		virtual void body_begin()
		{
		}

		virtual void body_end()
		{
		}

		virtual void process_end()
		{
		}

		virtual void header_end()
		{
		}

		bool header_ready() const
		{
			return header_ready_;
		}

		void reset_state()
		{
			request_info_.clear();
			current_parser_ = [this]() { return parse_first(); };
			finalized_ = false;
			expected_length_ = 0;
			header_ready_ = false;
			headers_.clear();
		}

		void reset_data()
		{
			full_data_.clear();
		}

		const header_map &headers() const
		{
			return headers_;
		}

		header_map &headers()
		{
			return headers_;
		}

		const container_type &data() const
		{
			return full_data_;
		}

		container_type &data()
		{
			return full_data_;
		}

		const std::vector<std::string> &info() const
		{
			return request_info_;
		}

		std::size_t expected_length() const
		{
			return expected_length_;
		}

		bool field_exists(const std::string &name) const
		{
			return headers_.count(name.c_str()) > 0;
		}

		bool reduce_data()
		{
			if (data().size() < expected_length_) {
				expected_length_ -= data().size();
				data().clear();
			}
			else {
				data().erase(data().begin(), data().begin() + expected_length_);
				reset_state();
				body_end();
			}
			return !data().empty();
		}

	protected:

		bool body_process_impl()
		{
			bool res = body_process();
			return res;
		}

		bool body_begin_impl()
		{
			body_begin();
			current_parser_ = [this]() { return body_process_impl(); };
			return true;
		}

		template <typename Itr>
		std::string tolower(Itr begin, Itr end)
		{
			std::string data(begin, end);
			for (auto &c : data) {
				c = std::tolower(c);
			}
			return data;
		}

		std::size_t expected_length_ = 0;

	private:

		bool parse_header_pair()
		{
			auto e = full_data_.end();
			auto b = tokenizer::skip_spaces(full_data_.begin(), e);

			if (b == e) {
				return false;
			}

			if (tokenizer::is_eol(*b)) {
				full_data_.erase(full_data_.begin(), std::next(b));
				current_parser_ = [this]() { return body_begin_impl();  };
				header_ready_ = true;
				header_end();
				return true;
			}

			auto next = tokenizer::skip_non_spaces_spacial(b, e);
			if (next == e) {
				return false;
			}
			auto key = std::make_pair(b, next);
			next = tokenizer::skip_spaces(next, e);
			if (next == e || *next != ':') {
				return false;
			}
			next = tokenizer::skip_spaces(std::next(next), e);
			b = tokenizer::skip_non_eol(next, e);
			if (b == e || !tokenizer::is_eol(*b)) {
				return false;
			}

			auto test_eol = b;
			while (b != next && tokenizer::is_space(*std::prev(b))) {
				--b;
			}

			std::string fld_name = tolower(key.first, key.second);
			std::string value(next, b);
			if (fld_name == "content-length") {
				try {
					expected_length_ = std::stoul(value);
				} catch (...) {
				}
			}

			headers_.insert(std::make_pair(std::move(fld_name), std::move(value)));

			full_data_.erase(full_data_.begin(), std::next(test_eol));

			return true;
		}

		bool parse_first()
		{
			auto e = full_data_.end();
			auto b = full_data_.begin();

			while (b != e) {
				b = tokenizer::skip_spaces(b, e);
				if ((b != e) && tokenizer::is_eol(*b)) {
					full_data_.erase(full_data_.begin(), std::next(b));
					current_parser_ = [this]() { return parse_header_pair(); };
					return true;
				}
				auto next = tokenizer::skip_non_spaces(b, e);
				if (next != e) {
					request_info_.emplace_back(std::string(b, next));
					b = next;
				}
				else {
					break;
				}
			}
			full_data_.erase(full_data_.begin(), b);
			return false;
		}

		container_type full_data_;
		header_map headers_;
		std::vector<std::string> request_info_;
		parser_call current_parser_;
		bool header_ready_ = false;
		bool finalized_ = false;
	};

	class header {
	protected:
		struct to_string {

			to_string(const std::string &val)
				:value(val)
			{}

			template <typename T>
			to_string(const T &val)
			{
				std::ostringstream oss;
				oss << val;
				value = oss.str();
			}
			std::string value;
		};

	public:

		virtual ~header() = default;
		header() = default;

		void add(std::initializer_list<std::pair<std::string, to_string> > vals)
		{
			for (auto &val : vals) {
				headers_.insert(std::make_pair(std::move(val.first),
					std::move(val.second.value)));
			}
		}

		void add(std::pair<std::string, to_string> val)
		{
			headers_.insert(std::make_pair(std::move(val.first),
				std::move(val.second.value)));
		}

		virtual std::string str() const
		{
			return str_headers();
		}

		std::string str_headers() const
		{
			std::ostringstream oss;
			for (auto &hdr : headers_) {
				oss << hdr.first << ": " << hdr.second << eol_value();
			}
			oss << eol_value();
			return oss.str();
		}
	protected:
		static const char *eol_value()
		{
			static const char values[3] = { '\r', '\n', '\0' };
			return values;
		}
	private:
		std::map<std::string, std::string> headers_;
	};

	class request : public header {
	public:
		request(std::string method, std::string path)
			:method_(std::move(method))
			, path_(std::move(path))
		{}

		std::string str() const override
		{
			std::ostringstream oss;
			oss << method_ << " " << path_ << " " << "HTTP/1.1" << eol_value();
			return oss.str() + str_headers();
		}

	private:
		std::string method_;
		std::string path_;
	};

	class response : public header {
	public:

		response(int code)
		{}

		response() = default;

		std::string str() const override
		{
			std::ostringstream oss;
			oss << "HTTP/1.1" << code_ << " " << "Code " << code_ << eol_value();
			return oss.str() + str_headers();
		}

	private:
		int code_ = 200;
	};
}