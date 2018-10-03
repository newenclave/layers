#pragma once

namespace test01 {

	namespace traits {
		struct raw_pointer {
			template <typename T>
			using pointer_type = T * ;

			template <typename T>
			static bool is_empty(pointer_type<T> ptr)
			{
				return ptr == nullptr;
			}

			template <typename T>
			static pointer_type<T> get_write(pointer_type<T> ptr)
			{
				return ptr;
			}

			template <typename T>
			static void destroy(pointer_type<T>)
			{
			}

			template <typename T>
			static void swap(pointer_type<T> &lh, pointer_type<T> &rh)
			{
				std::swap(lh, rh);
			}
		};

		struct unique_pointer {
			template <typename T>
			using pointer_type = std::unique_ptr<T>;

			template <typename T>
			static bool is_empty(pointer_type<T> &ptr)
			{
				return ptr == nullptr;
			}

			template <typename T>
			static pointer_type<T> &get_write(pointer_type<T> &ptr)
			{
				return ptr;
			}

			template <typename T>
			static void destroy(pointer_type<T> &ptr)
			{
				ptr.reset();
			}

			template <typename T>
			static void swap(pointer_type<T> &lh, pointer_type<T> &rh)
			{
				std::swap(lh, rh);
			}
		};
	}

	template <typename MsgType, 
		typename UpperPtrTrait = traits::raw_pointer,
		typename LowerPtrTrait = UpperPtrTrait
	>
	class layer {
		using upper_traits = UpperPtrTrait;
		using lower_traits = LowerPtrTrait;
		using this_type = layer<MsgType, upper_traits, lower_traits>;
	public:
		using message_type = MsgType;
		using upper_pointer_type =
			typename upper_traits::template pointer_type<this_type>;
		using lower_pointer_type =
			typename lower_traits::template pointer_type<this_type>;

		virtual ~layer()
		{
			upper_traits::destroy(upper_);
			lower_traits::destroy(lower_);
		}

		layer() = default;

		layer(layer<MsgType, upper_traits, lower_traits> &&other)
		{
			swap(other);
		}

		layer& operator = (layer<MsgType, upper_traits, lower_traits> &&other)
		{
			swap(other);
			return *this;
		}

		void swap(layer<MsgType, upper_traits, lower_traits> &other)
		{
			upper_traits::swap(upper_, other.upper_);
			lower_traits::swap(lower_, other.lower_);
		}

		void set_upper(upper_pointer_type upper)
		{
			std::swap(upper_, upper);
		}

		void set_lower(lower_pointer_type lower)
		{
			std::swap(lower_, lower);
		}

	public:

		virtual void from_upper(message_type msg) // from upper layer
		{
			send_to_lower(std::move(msg));
		}
		virtual void from_lower(message_type msg) // from lower layer
		{
			send_to_upper(std::move(msg));
		}

	protected:
		void send_to_lower(message_type msg)
		{
			lower_traits::get_write(lower_)->from_upper(std::move(msg));
		}
		void send_to_upper(message_type msg)
		{
			upper_traits::get_write(upper_)->from_lower(std::move(msg));
		}

		bool has_upper() const
		{
			return !upper_traits::is_empty(upper_);
		}

		bool has_lower() const
		{
			return !lower_traits::is_empty(lower_);
		}
	private:
		upper_pointer_type upper_ = nullptr;
		lower_pointer_type lower_ = nullptr;
	};
}
