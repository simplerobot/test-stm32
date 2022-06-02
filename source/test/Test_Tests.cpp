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

