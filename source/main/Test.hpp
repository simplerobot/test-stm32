#pragma once


#ifndef __cplusplus
#error Test cases must be compiled with c++ support.
#endif

#ifndef TEST
#error Tests must be compiled with the TEST macro defined.
#endif

#ifndef __EXCEPTIONS
#error Tests must be compiled with exception support.
#endif


#include "Assert.h"


#define TEST_CASE(X) \
	void TEST_CASE_ ## X(); \
	TestCaseListItem _TEST_ ## X(TEST_CASE_ ## X, #X, __FILE__, __LINE__); \
	void TEST_CASE_ ## X()


class TestCaseListItem
{
public:
	typedef void(*TestCase)();

	TestCaseListItem(TestCase test, const char* name, const char* file, long line);
	~TestCaseListItem();

	bool Run();
	static bool RunAll();

private:
	TestCase m_test;
	const char* m_name;
	const char* m_file;
	long m_line;
	TestCaseListItem* m_next;
	TestCaseListItem** m_prev;

	static TestCaseListItem* g_head;
	static TestCaseListItem** g_tail;
};


#define TEST_SETUP(X) TEST_HELPER(X, SETUP)
#define TEST_TEARDOWN(X) TEST_HELPER(X, TEARDOWN)
#define TEST_START(X) TEST_HELPER(X, START)
#define TEST_FINISH(X) TEST_HELPER(X, FINISH)

#define TEST_HELPER(NAME, TYPE) \
	void TEST_ ## TYPE ## _ ## NAME(); \
	TestHelperListItem _TEST_ ## TYPE ## _ ## NAME(TEST_ ## TYPE ## _ ## NAME, TestHelperListItem::TYPE); \
	void TEST_ ## TYPE ## _ ## NAME()


class TestHelperListItem
{
public:
	enum Type
	{
		SETUP,
		TEARDOWN,
		START,
		FINISH,
	};

	typedef void(*HelperFn)();

	TestHelperListItem(HelperFn fn, Type type);
	~TestHelperListItem();

	static void Run(Type type);

private:
	HelperFn m_helper_fn;
	Type m_type;
	TestHelperListItem* m_next;
	TestHelperListItem** m_prev;

	static TestHelperListItem* g_head;
	static TestHelperListItem** g_tail;
};

