#pragma once

namespace test01 {

	namespace traits {
		struct raw_pointer {
			template <typename T>
			using pointer_type = T * ;

			template <typename T>
			static bool is_empty(pointer_type<T> ptr)
			{
				return ptr != nullptr;
			}

			template <typename T>
			static pointer_type<T> get_write(pointer_type<T> ptr)
			{
				return ptr;
			}
		};
	}

	template <typename MsgType, typename PtrTraits = traits::raw_pointer>
	class layer {
		using traits_type = PtrTraits;
	public:
		using this_type = layer<MsgType, traits_type>;
		using message_type = MsgType;
		using pointer_type = typename traits_type::template pointer_type<this_type>;

		virtual ~layer() = default;
		virtual void from_upper(message_type msg) // from upper layer
		{
			send_to_lower(std::move(msg));
		}
		virtual void from_lower(message_type msg) // from lower layer
		{
			send_to_upper(std::move(msg));
		}
		void set_upper(pointer_type upper)
		{
			std::swap(upper_, upper);
		}
		void set_lower(pointer_type lower)
		{
			std::swap(lower_, lower);
		}

	protected:
		void send_to_lower(message_type msg)
		{
			traits_type::get_write(lower_)->from_upper(std::move(msg));
		}
		void send_to_upper(message_type msg)
		{
			traits_type::get_write(upper_)->from_lower(std::move(msg));
		}

		bool has_upper() const
		{
			return traits_type::is_empty(upper_);
		}

		bool has_lower() const
		{
			return traits_type::is_empty(lower_);
		}
	private:
		pointer_type upper_ = nullptr;
		pointer_type lower_ = nullptr;
	};
}
