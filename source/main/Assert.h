#pragma once


#ifdef TEST
	#define FAIL(...) NotifyAssertFailed(__FILE__, __LINE__, __PRETTY_FUNCTION__, __VA_ARGS__)
	#define ASSERT(X) ((X) ? (void)0 : FAIL("ASSERT(%s)", #X))
	#define ASSERT_TRUE(X) ((X) ? (void)0 : FAIL("ASSERT_TRUE(%s)", #X))
	#define ASSERT_FALSE(X) ((X) ? FAIL("ASSERT_FALSE(%s)", #X) : (void)0)
	#define ASSERT_THROWS(X) { bool caught_error = false; try { X; } catch(const AssertFailedException&) { throw; } catch (...) { caught_error = true; } if (!caught_error) FAIL("ASSERT_THROWS(%s)", #X); }
	#define ASSERT_ASSERTS(X) { bool caught_assert = false; try { X; } catch(const AssertFailedException&) { caught_assert = true; } if (!caught_assert) FAIL("ASSERT_ASSERTS(%s)", #X); }

	#ifdef __cplusplus
	extern "C" {
	#endif
	extern void NotifyAssertFailed(const char* file, long line, const char* function, const char* message, ...) __attribute__ ((format (printf, 4, 5)));
	#ifdef __cplusplus
	}
	#endif

	#ifdef __cplusplus

	class AssertFailedException
	{
	public:
		AssertFailedException() = default;
	};

	#endif

#else
	#define FAIL(...)
	#define ASSERT(X)
	#define ASSERT_TRUE(X)
	#define ASSERT_FALSE(X)
	#define ASSERT_THROWS(X)
#endif
