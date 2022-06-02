#include "Test.hpp"
#include "main.h"
#include <unistd.h>
#include <exception>
#include <cmsis_os.h>
#include <cstring>


TestCaseListItem* TestCaseListItem::g_head = nullptr;
TestCaseListItem** TestCaseListItem::g_tail = &g_head;
static int g_test_depth = 0;
static bool g_test_failure = false;
TaskHandle_t g_testing_thread_id = 0;


static void terminate_fn()
{
	ITM_Format("terminate\n");
	while (1);
}

namespace __cxxabiv1 {
	std::terminate_handler __terminate_handler = terminate_fn;
}

extern "C" void RLM3_Main(void)
{
	TestCaseListItem::RunAll();
	ITM_Format("EOT FAIL\n");
}

class AssertFailedException
{
public:
	AssertFailedException() {}
};

TestCaseListItem::TestCaseListItem(TestCase test, const char* name, const char* file, long line)
	: m_test(test)
	, m_name(name)
	, m_file(file)
	, m_line(line)
	, m_next(nullptr)
	, m_prev(g_tail)
{
	*g_tail = this;
	g_tail = &this->m_next;
}

TestCaseListItem::~TestCaseListItem()
{
	*m_prev = m_next;
}

bool TestCaseListItem::Run()
{
	g_test_depth++;
	g_test_failure = false;
	bool test_passed = false;
	g_testing_thread_id = xTaskGetCurrentTaskHandle();
	try
	{
		(m_test)();
		test_passed = !g_test_failure;
	}
	catch (const AssertFailedException& e)
	{
	}
	catch (const std::exception& e)
	{
		const char* name = typeid(e).name();
		ITM_Format("FAILED - Test failed with exception %s.  Error: %s\n", name, e.what());
	}
	catch (...)
	{
		ITM_Format("FAILED - Test failed with unknown exception.\n");
	}
	g_test_depth--;
	g_test_failure = false;
	g_testing_thread_id = 0;

	return test_passed;
}

bool TestCaseListItem::RunAll()
{
	ITM_Format("== RUNNING TEST CASES ==\n");

	size_t total_test_count = 0;
	size_t passed_test_count = 0;

	for (TestCaseListItem* current_test = g_head; current_test != nullptr; current_test = current_test->m_next)
	{
		ITM_Format("=== TEST: %s ===\n", current_test->m_name);

		total_test_count++;

		if (current_test->Run())
		{
			passed_test_count++;
		}
		else
		{
			ITM_Format("=== TEST FAILED: %s File '%s' line %d ===\n", current_test->m_name, current_test->m_file, current_test->m_line);
		}
	}

	ITM_Format("== TEST SUMMARY ==\n");
	ITM_Format("%d Total Tests\n", total_test_count);
	ITM_Format("%d Tests Passed\n", passed_test_count);
	if (total_test_count == passed_test_count)
	{
		ITM_Format("== TESTS PASSED ==\n");
		ITM_Format("EOT PASS\n");
		return true;
	}
	else
	{
		ITM_Format("%d Failed Tests\n", total_test_count - passed_test_count);
		ITM_Format("== TESTS FAILED ==\n");
		return false;
	}
}

static const char* ShortFileName(const char* filename)
{
	const char* result = filename;
	for (const char* cursor = filename; *cursor != 0; cursor++)
		if (*cursor == '/' || *cursor == '\\')
			result = cursor + 1;
	return result;
}

static bool is_destructor(const char* function_name)
{
	return (std::strstr(function_name, "::~") != nullptr);
}

extern void NotifyAssertFailed(const char* file, long line, const char* function, const char* message, ...)
{
	bool throw_error = true;
	const char* failure_type = "ASSERT FAILED";

	if (g_test_depth == 0)
	{
		failure_type = "ASSERT FAILED OUTSIDE TESTS";
		throw_error = false;
	}

	if (g_test_depth > 1)
	{
		failure_type = "NESTED ASSERT FAILED";
	}

	if (g_testing_thread_id != xTaskGetCurrentTaskHandle())
	{
		failure_type = "ASSERT FAILED IN SECONDARY THREAD";
		g_test_failure = true;
		throw_error = false;
	}

	if (std::uncaught_exception())
	{
		failure_type = "ASSERT FAILED IN EXCEPTION PROCESSING";
		g_test_failure = true;
		throw_error = false;
	}

	if (is_destructor(function))
	{
		failure_type = "ASSERT FAILED IN DESTRUCTOR";
		g_test_failure = true;
		throw_error = false;
	}

	va_list args;
	va_start(args, message);
	ITM_Format("%s '", failure_type);
	ITM_VFormat(message, args);
	ITM_Format("' %s %s:%d\n", function, ShortFileName(file), line);

	if (throw_error)
	{
		throw AssertFailedException();
	}
}


//extern "C" void __cxa_pure_virtual()
//{
//	ITM_Format("pure virtual\n");
//	while(1);
//}
//
//void* operator new(size_t size)
//{
//	return pvPortMalloc(size);
//}
//
//void operator delete(void* ptr)
//{
//	if (ptr != nullptr)
//		vPortFree(ptr);
//}

extern "C" void _exit(int result)
{
	ITM_Format("exit\n");
	while(1);
}

extern "C" pid_t _getpid(void)
{
	return 0;
}

extern "C" int _kill(pid_t pid, int sig)
{
	return 0;
}

extern void* DANGER_sbrk_was_linked_into_application();

extern "C" void* _sbrk(ptrdiff_t incr)
{
	return DANGER_sbrk_was_linked_into_application();
}

extern "C" void* _malloc_r(struct _reent*, size_t size)
{
	return pvPortMalloc(size);
}

extern "C" void _free_r(struct _reent*, void *p)
{
	vPortFree(p);
}
