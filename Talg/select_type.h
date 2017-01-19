﻿#include "core.h"
#include "invoke_impl.h"
#include "index_seq.h"

/*
	\brief	具有接受任意参数的构造子,并且不做任何事
	\param	anything
	\note	典型地,用于忽略函数调用的参数从而实现各种编译期元操作,参见@get
*/
struct EatParam {
	constexpr EatParam(...)noexcept {}
};
template<class>
using EatParam_t = EatParam;



/*
	\brief	产生n个EatParam组成的序列
	\param	n表示EatParam的个数
	\return	n==0时产生Seq<>,n==1时产生Seq<EatParam>,n==2时产生Seq<EatParam,EatParam> 类推.
	\note	采用了二分实现来减少实例化深度.
*/
template<size_t n>
struct IgnoreSeqImp {
	using type = Merge_s<OMIT_T(IgnoreSeqImp<n / 2>), OMIT_T(IgnoreSeqImp<n - n / 2>)>;
};
template<>
struct IgnoreSeqImp<0> {
	using type = Seq<>;
};
template<>
struct IgnoreSeqImp<1> {
	using type = Seq<EatParam>;
};


/*
	\brief	产生n个EatParam组成的序列
*/
template<size_t n>
using IgnoreSeq = OMIT_T(IgnoreSeqImp<n>);


/*
	\brief	高效实现各种选择相关的模板元操作
	\param	Ts...被划分出的类型,它们可能被丢弃(从而直接使用未丢弃的部分)或直接使用.
*/
template<class...Ts>
struct GetImp {

	/*
		\brief	用于高效实现get<n>(args...)
		\param	从0开始,选取第n个参数
		\return	完美转发选择的参数
	*/
	template<class T>
	static constexpr decltype(auto) fetch(Ts&&..., T&& obj, ...)noexcept {
		return std::forward<T>(obj);
	}


	/*
		\brief	用于实现选择第n个类型
		\param	从0开始的第n个
		\return	返回一个包装着所选类型的类
		\note	无法直接返回T,如果T构造子被删
	*/
	template<class T>
	static WrapperT<T> deduce(Ts..., Seq<T>* obj, ...);


	/*
		\brief	对前面的参数,也即Ts...应用函子.
		\param	\func 函子	
				\args... 函调用时使用的参数
		\return	同func的返回值一样
	*/
	template<class F>
	static constexpr decltype(auto) applyFrontImp(F&& func, Ts&&...args, ...)
		noexcept(noexcept(ct_invoke(forward_m(func), forward_m(args)...)))
	{
		return ct_invoke( forward_m(func),forward_m(args)...);
	}


	/*
		\brief	对后面的参数应用函子,忽略Ts....
		\param	\func 函子	
				\args... 函调用时的使用的参数
		\return	同func
	*/
	template<class F, class...Us>
	static constexpr decltype(auto) applyBackImp(F&& func, Ts&&..., Us&&...args)
		noexcept(noexcept(ct_invoke(forward_m(func), forward_m(args)...)))
	{
		return ct_invoke(forward_m(func), forward_m(args)...);
	}
};







/*
	\brief	产生GetImp<EatParam...> 其中有EatParam n个.
	\param	\n 个数 0表示没有,从1数起.
	\example	参见@get,利用ExcludeParam<n>排除掉参数表的前面n个参数,剩下的就是从0数起的第n个参数
*/
template<size_t n>
using ExcludeParam = Transform<GetImp, IgnoreSeq<n>>;


/*
	\brief	从参数表中选择第n个参数,从0数起
	\return	完美转发所选择的参数
	\note	参见@ExcludeParam
*/
template<size_t n, class...Ts>
constexpr decltype(auto) get(Ts&&...args)noexcept {
	static_assert(n<sizeof...(args), "index over range.");
	return ExcludeParam<n>::fetch(forward_m(args)...);
}


/*
	\brief	从Seq<...>中选择第n个类型,从0数起
	\param	\n index 
			\T Seq<...>
*/
template<size_t n, class T>struct AtImp;
template<size_t n, class...Ts>
struct AtImp<n, Seq<Ts...>> {
	using tmp = decltype(
		ExcludeParam<n>::deduce(static_cast<Seq<Ts>*>(0)...)
	);
	using type = typename tmp::type;
};

/*
	\brief	返回Seq<???>的???中的第n个类型
	\param	\n index
*/
template<size_t n, class T>
using At_s = OMIT_T(AtImp<n, T>);

/*
	\brief	将 ???<???>转为Seq<???> 返回???中的第n个类型
	\param	\n index
*/
template<size_t n, class T>
using At = OMIT_T(AtImp<n, Seqfy<T>>);

/*
	\brief	把函子施加到前n个参数
	\param	\func	被调用的函子
			\args	参数表
	\return	同func
	\note	相当于ct_invoke(func,get<0>(args...),get<1>(args...), .... , get<n-1>(args...))
			的高效实现
*/
template<size_t n, class F, class...Ts>
constexpr decltype(auto) applyFront(F&& func, Ts&&... args) {
	return Transform<GetImp, Before_s<n, Seq<Ts...>> >
		::applyFrontImp(forward_m(func), forward_m(args)...);
}

/*
	\brief	忽略前n个参数,把函子施加于后面的全部参数
	\param	\IgnoreCount	忽略的参数个数 
			\func	所用的函子,
			\args	所有参数,包含了被忽略的及要用的.
	\return	同func
	\example applyBack<3>( print, 1,2,3,4,5,6)
			 等效于 print(4);print(5); return print(6);
*/
template<size_t IgnoreCount, class F, class...Ts>
constexpr decltype(auto) applyBack(F&& func, Ts&&... args) {
	return ExcludeParam<IgnoreCount>::applyBackImp(forward_m(func), forward_m(args)...);
}


/*
	\brief	对任意下标的参数进行函数调用
	\param	\func	所用的函子,
			\args	所有参数,包含了被忽略的及要用的.
	\return	同func
	\example applyRag(IdSeq<0,3,4,1>{}, print, 0,1,2,3,4,5,6)
			 等效于 print(0);print(3);print(4);return print(1);
*/
template<size_t... Indices, class F, class...Ts>
constexpr decltype(auto) apply(IdSeq<Indices...>, F&& func, Ts&&... args) {
	return ct_invoke(forward_m(func), get<Indices>(forward_m(args)...)...);
}
template<class F, class...Ts>
constexpr decltype(auto) apply(IdSeq<>, F&& func, Ts&&... args) {
	return forward_m(func)();
}
template<class F, class...Ts>
constexpr decltype(auto) apply(Seq<>, F&& func, Ts&&... args) {
	return ct_invoke(forward_m(func), forward_m(args)...);
}






template<class...Ts>
struct ApplyRagImp {
	template<size_t end, class F, class...Us>
	static constexpr decltype(auto) call(Tagi<end>&&, F&& func, Ts&&..., Us&&...args) {
		return applyFront<end>(forward_m(func), forward_m(args)...);
	}
};


/*
	\brief	丢弃前beg个参数,从而对[beg,end)范围的参数施加函子
	\param	\func	所用的函子,
			\args	所有参数,包含了被忽略的及要用的.
	\return	同func	
	\example applyRag(IdSeq<2,5>{}, 
					[](int a,int b,int c){cout<<a<<b<<c;}, 0,1,2,3,4,5,6)
			 输出 234
	\note
*/
template<size_t beg, size_t end, class F, class...Ts>
constexpr decltype(auto) applyRag(IdSeq<beg,end>&&, F&& func, Ts&&... args) {
	return Transform<ApplyRagImp, IgnoreSeq<beg> >
		::call(Tagi<end - beg >{}, forward_m(func), forward_m(args)...);
}

template<class T>
struct ApplyRag;

template<size_t beg,size_t end>
struct ApplyRag<IdSeq<beg,end>> {
	using caller = Transform<ApplyRagImp, IgnoreSeq<beg> >;
	template<class F, class...Ts>
	static constexpr decltype(auto) call(F&& func, Ts&&... args) {
		return caller::call(Tagi<end - beg >{}, forward_m(func), forward_m(args)...);
	}
};


template<size_t IntervalCount,size_t IntervalLen,size_t Start=0>
struct IntervalPartion_st {
	using type =
		Merge_s<
			Seq<IdSeq<Start,Start+IntervalLen>>,
			typename IntervalPartion_st<IntervalCount-1,IntervalLen, Start+IntervalLen>::type
		>;
};
template<size_t Len,size_t Start>
struct IntervalPartion_st<0,Len,Start> {
	using type = Seq<>;
};

template<size_t TotalRag,size_t IntervalLen>
using IntervalPartion_s = typename IntervalPartion_st<TotalRag,IntervalLen>::type;


/*
	\brief	把一个Seq分组,每组都是一个Seq,每组有n个类型
	\param	\N 为每组的类型个数 \T要求是一个Seq<??>
	\example Partion<2,Seq<A,B,C,D>> is  Seq< Seq<A,B>,Seq<C,D> >
*/
template<size_t N,class T>
struct PartionImp{
	using type = Merge_s< 
		Seq<Before_s<N, T>>, 
		typename PartionImp<N, After_s<N - 1, T>>::type
	>;
};
template<size_t N>
struct PartionImp<N,Seq<>>
{
	using type = Seq<>;
};
template<size_t N,class T>
using Partion_s = typename PartionImp<N, T>::type;
template<size_t N,class T>
using Partion = Partion_s<N, Seqfy<T>>;




template<class T>
struct GatherImp {};
template<class... Ts>
struct GatherImp<Seq<Ts...>> { 
	template<class Receiver,class...Us>
	static constexpr decltype(auto) call(Receiver&& dst, Us&&...args) {
		//fix: 此处由于使用了decltype(auto)而没有通过编译,原因不详.可改成auto,但那会导致运行期语义不对.
		return ct_invoke( forward_m(dst), applyRag(Ts{},forward_m(args)...)...);
	}
};

/*
	\brief	以一个函子接收另一个函子的多次调用的返回结果,
	\param	args[0] is receiver, args[1] is the functor which be mapped.
	\return	as like receiver( func( real_args...)... )
			which receiver is args[0], func is args[1], real_args are other args.
	\example	
				auto show=[](auto&&...arg){
					mapAny([](auto e){cout<<e;},args...);
				};
				auto double_val=[](auto&& arg){ return arg*2; };
				auto sum=[](auto lhs,auto rhs){ return lhs+rhs;}
				gather<1>(show,double_val,0,1,2,3);  //show(double_val(0),double_val(1),double_val(2),double_val(3))
				cout<<endl;
				gather<2>(show,sum,0,1,  2,3);  //show(sum(0,1),sum(2,3))
				output:
					0246
					15
	todo fix: 函数调用时若参数类型不匹配,会造成成吨的编译错误,比较难找到原因
				比如auto double_val=[](auto& arg){ return arg*2; };是非法的
				但是整份编译错误过于冗长,并且错误位点没有定位到调用上,而是错误地引进
				了过多的实例化上下文,以至于关键信息被掩盖,上述写法错误理由其实很简单
				不可将1之类的常量转换为auto&, 改成auto&& 或者 const auto&皆可
*/
template<size_t FuncArgCount,class...Ts>
constexpr decltype(auto) gather(Ts&&...args) {
	static_assert((sizeof...(args)-2) % FuncArgCount == 0, "group size no match total size.");
	return GatherImp<IntervalPartion_s<(sizeof...(args)-2)/FuncArgCount,FuncArgCount>>::call(forward_m(args)...);
}