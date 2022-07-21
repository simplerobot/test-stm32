#include "Test.hpp"
#include <stdexcept>
#include "cmsis_os2.h"


TEST_CASE(TestCaseListItem_Constructor_HappyCase)
{
	TestCaseListItem test([](){}, "TEST_CONSTRUCTOR", "FILE", 1234);
}

TEST_CASE(TestCaseListItem_Run_HappyCase)
{
	TestCaseListItem test([](){}, "TEST_CONSTRUCTOR", "FILE", 1234);

	ASSERT(test.Run());
}

TEST_CASE(TestCaseListItem_Run_TestFails)
{
	TestCaseListItem test([](){ ASSERT(false); }, "TEST_CONSTRUCTOR", "FILE", 1234);

	ASSERT(!test.Run());
}

TEST_CASE(ASSERT_Passes)
{
	auto test = []() { ASSERT(true); };
	TestCaseListItem test_case(test, "TEST_CONSTRUCTOR", "FILE", 1234);

	ASSERT(test_case.Run());
}

TEST_CASE(ASSERT_Fails)
{
	auto test = []() { ASSERT(false); };
	TestCaseListItem test_case(test, "TEST_CONSTRUCTOR", "FILE", 1234);

	ASSERT(!test_case.Run());
}

TEST_CASE(ASSERT_TRUE_Passes)
{
	auto test = []() { ASSERT_TRUE(true); };
	TestCaseListItem test_case(test, "TEST_CONSTRUCTOR", "FILE", 1234);

	ASSERT(test_case.Run());
}

TEST_CASE(ASSERT_TRUE_Fails)
{
	auto test = []() { ASSERT_TRUE(false); };
	TestCaseListItem test_case(test, "TEST_CONSTRUCTOR", "FILE", 1234);

	ASSERT(!test_case.Run());
}

TEST_CASE(ASSERT_FALSE_Passes)
{
	auto test = []() { ASSERT_FALSE(false); };
	TestCaseListItem test_case(test, "TEST_CONSTRUCTOR", "FILE", 1234);

	ASSERT(test_case.Run());
}

TEST_CASE(ASSERT_FALSE_Fails)
{
	auto test = []() { ASSERT_FALSE(true); };
	TestCaseListItem test_case(test, "TEST_CONSTRUCTOR", "FILE", 1234);

	ASSERT(!test_case.Run());
}

TEST_CASE(ASSERT_THROWS_Passes)
{
	auto test = []() { ASSERT_THROWS(throw std::runtime_error("error")); };
	TestCaseListItem test_case(test, "TEST_CONSTRUCTOR", "FILE", 1234);

	ASSERT(test_case.Run());
}

TEST_CASE(ASSERT_THROWS_Fails)
{
	auto test = []() { ASSERT_THROWS((void)0); };
	TestCaseListItem test_case(test, "TEST_CONSTRUCTOR", "FILE", 1234);

	ASSERT(!test_case.Run());
}

TEST_CASE(ASSERT_THROWS_Asserts)
{
	auto test = []() { ASSERT_THROWS(ASSERT(false)); };
	TestCaseListItem test_case(test, "TEST_CONSTRUCTOR", "FILE", 1234);

	ASSERT(!test_case.Run());
}

TEST_CASE(ASSERT_ASSERTS_Passes)
{
	auto test = []() { ASSERT_ASSERTS(ASSERT(false)); };
	TestCaseListItem test_case(test, "TEST_CONSTRUCTOR", "FILE", 1234);

	ASSERT(test_case.Run());
}

TEST_CASE(ASSERT_ASSERTS_Fails)
{
	auto test = []() { ASSERT_ASSERTS(ASSERT(true)); };
	TestCaseListItem test_case(test, "TEST_CONSTRUCTOR", "FILE", 1234);

	ASSERT(!test_case.Run());
}

TEST_CASE(ASSERT_ASSERTS_Throws)
{
	auto test = []() { ASSERT_ASSERTS(throw std::runtime_error("error")); };
	TestCaseListItem test_case(test, "TEST_CONSTRUCTOR", "FILE", 1234);

	ASSERT(!test_case.Run());
}

TEST_CASE(ASSERT_Fails_Secondary_Thread)
{
	auto test = []() {
		auto secondary_thread_fn = [](void* param)
		{
			bool expected_failure_in_secondary_thread = false;
			ASSERT(expected_failure_in_secondary_thread);
			*(bool*)param = true;
			while (true) ;
		};

		bool task_complete_flag = false;

		osThreadAttr_t task_attributes = {};
		task_attributes.name = "secondary_thread";
		task_attributes.stack_size = 128 * 4;
		task_attributes.priority = osPriorityNormal;

		osThreadId_t secondary_thread = ::osThreadNew(secondary_thread_fn, &task_complete_flag, &task_attributes);
		if (secondary_thread != nullptr)
		{
			while (!task_complete_flag)
				::osDelay(1);

			::osThreadTerminate(secondary_thread);
		}
	};
	TestCaseListItem test_case(test, "TEST_CONSTRUCTOR", "FILE", 1234);

	ASSERT(!test_case.Run());
}

TEST_CASE(ASSERT_Fails_Destructor)
{
	class DestructorFailure
	{
	public:
		~DestructorFailure() { ASSERT(false); }
	};

	auto test = []() {
		DestructorFailure test;
	};
	TestCaseListItem test_case(test, "TEST_CONSTRUCTOR", "FILE", 1234);

	ASSERT(!test_case.Run());
}

static uint32_t g_total_setup_count = 0;
static uint32_t g_total_teardown_count = 0;
static uint32_t g_active_count = 0;

TEST_SETUP(COUNTS)
{
	g_total_setup_count++;
	g_active_count++;
}

TEST_TEARDOWN(COUNTS)
{
	ASSERT(g_active_count >= 0);
	g_total_teardown_count++;
	g_active_count--;
}

TEST_CASE(TEST_HELPER_Counts)
{
	auto test = []() { ASSERT(g_active_count == 2); };
	TestCaseListItem test_case(test, "NESTED_TEST", "FILE", 1234);

	ASSERT(g_active_count == 1);
	ASSERT(test_case.Run());
	ASSERT(g_active_count == 1);
}

static bool g_should_start_fn_fail = false;
static bool g_should_finish_fn_fail = false;

TEST_START(FAILS)
{
	if (g_should_start_fn_fail)
	{
		g_should_start_fn_fail = false;
		ASSERT(false);
	}
}

TEST_FINISH(FAILS)
{
	if (g_should_finish_fn_fail)
	{
		g_should_finish_fn_fail = false;
		ASSERT(false);
	}
}

TEST_CASE(TEST_HELPER_Fails)
{
	TestCaseListItem test_case([]{}, "ALWAYS_PASSES", "FILE", 1234);

	g_should_start_fn_fail = true;
	ASSERT(!test_case.Run());

	ASSERT(test_case.Run());

	g_should_finish_fn_fail = true;
	ASSERT(!test_case.Run());
}



