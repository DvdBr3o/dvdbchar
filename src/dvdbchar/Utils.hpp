#pragma once

namespace dvdbchar {
	template<typename T>
	struct analyze_member_ptr {};

	template<typename T, typename R>
	struct analyze_member_ptr<R T::*> {
		using type		  = T;
		using member_type = R;
	};
}  // namespace dvdbchar