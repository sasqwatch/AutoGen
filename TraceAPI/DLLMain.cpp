//
// FACILITY:	TraceAPI - Trace specific APIs when injected into a process
//
// DESCRIPTION:	This DLL is injected into a process by InjectDLL or WithDLL. Its purpose is to intercept specific APIs and log their parameters using 
//				ETW
//
// VERSION:		1.0
//
// AUTHOR:		Brian Catlin
//
// CREATED:		2019-11-15
//
// MODIFICATION HISTORY:
//
//	1.0		2019-11-15	Brian Catlin
//			Original version
//

#pragma warning (disable : 4100)						// Allow unreferenced formal parameter
#pragma warning (disable : 4115)						// Allow named type definition in parentheses
#pragma warning (disable : 4127)						// Allow constant conditional expression
#pragma warning (disable : 4200)						// Allow zero-sized array in struct/union
#pragma warning (disable : 4201)						// Allow nameless struct/union
#pragma warning (disable : 4214)						// Allow bit field types other than int
#pragma warning (disable : 4514)						// Allow unreferenced inline function

//
// INCLUDE FILES:
//

//
// System includes
//

#define WIN32_NO_STATUS
#include <Windows.h>
#undef WIN32_NO_STATUS
#include <ntstatus.h>
#include <TraceLoggingProvider.h>
#include <evntprov.h>
#include <psapi.h>

#include <string>
#include <vector>
#include <list>
#include <memory>
#include <iterator>

//
// Project includes
//

#include "detours.h"

#include "TraceAPI.h"
#include "..\Global\Utils.h"
#include "FDI-Detours.h"
#include "..\Global\WPP_Tracing.h"
#include "Version.h"

#include "DLLMain.tmh"								// Generated by TraceWPP

using namespace FDI;

//
// CONSTANTS:
//

//
// TYPES:
//

//
// MACROS:
//

//
// DEFINITIONS:
//

//
// DECLARATIONS:
//

TRACELOGGING_DEFINE_PROVIDER (TA_tlg, TL_PROVIDER, TL_GUID);
static LONG		TA_tls_indent = -1;
static LONG		TA_tls_thread = -1;
static LONG		TA_thread_count = 0;
WCHAR			TA_image_file_name_buff [MAX_PATH];
UNICODE_STRING	TA_image_file_name = {0};

const char	TA_product_description[] = VER_PRODUCTNAME_STR " " VER_INTERNALNAME_STR " v" VER_PRODUCTVERSION_STR " " VER_BUILD_TYPE_STR ", Built: " __DATE__ " " __TIME__;

//
// FORWARD ROUTINES:
//

VOID
det_attach												// Attach the Detours
	(
	_In_	PVOID	*Real_api,							// The real API
	_In_	PVOID	My_api,								// My Detoured API
	_In_	PCCH	Api_name							// API name
	);

VOID
det_detach												// Detach the Detours
	(
	_In_	PVOID	*Real_api,							// The real API
	_In_	PVOID	My_api,								// My Detoured API
	_In_	PCCH	Api_name							// API name
	);

PCCH
det_real_name											// 
	(
	_In_	PCCH Name									// 
	);

BOOL 
process_attach											// Called when this DLL is loaded into a process
	(
	_In_	HMODULE		Dll_hdl							// This DLL's module handle							
	);

BOOL 
process_detach											// Called when this DLL is unloaded from a process
	(
	_In_	HMODULE		Dll_hdl							// This DLL's module handle							
	);

BOOL
thread_attach											// Called when the process creates a new thread
	(
	_In_	HMODULE		Dll_hdl							// This DLL's module handle							
	);

BOOL 
thread_detach											// Called when a thread is exiting cleanly
	(
	_In_	HMODULE		Dll_hdl							// This DLL's module handle							
	);



BOOL 
APIENTRY 
DllMain													// Called when the DLL is loaded or unloaded from a process
	(
	_In_	HMODULE	Module_handle,						// Handle to this DLL
	_In_	DWORD	Call_reason,						// Reason for being called
	_In_	LPVOID	Reserved							// Reserved
	)

//
// DESCRIPTION:		This routine is called automatically by the image loader when this DLL is loaded
//
// ASSUMPTIONS:		User mode
//
// SIDE EFFECTS:
//
// RETURN VALUES:
//
//  TRUE						Normal, successful completion
//

{
BOOL	ret_val;


	switch (Call_reason)
		{
		case DLL_PROCESS_ATTACH:
			{
			DetourRestoreAfterWith ();

			ret_val = process_attach (Module_handle);
			}
			break;

		case DLL_PROCESS_DETACH:
			{
			ret_val = process_detach (Module_handle);
			}
			break;

		case DLL_THREAD_ATTACH:
			{
			ret_val = thread_attach (Module_handle);
			}
			break;

		case DLL_THREAD_DETACH:
			{
			ret_val = thread_detach (Module_handle);
			}
			break;

		default:
			{
			}
			break;
		}

	return TRUE;
}									// End routine DllMain


VOID
det_attach												// Attach the Detours
	(
	_In_	PVOID	*Real_api,							// The real API
	_In_	PVOID	My_api,								// My Detoured API
	_In_	PCCH	Api_name							// API name
	)

//
// DESCRIPTION:		
//
// ASSUMPTIONS:		User mode
//
// SIDE EFFECTS:
//
// RETURN VALUES:
//
//

{
NTSTATUS	status;


	TRACE_ENTER ();
	TRACE_INFO (TRACEAPI, "Detouring API %s", Api_name);

	//
	// Initialize the Detours library
	//

	if (ERR (status = DetourAttach (Real_api, My_api)))
		{
		TRACE_WARN (TRACEAPI, "Attach failed: `%s', status = %!STATUS!", det_real_name (Api_name), status);
		}

	TRACE_EXIT ();
	}							// End det_attach


VOID
det_detach												// Detach the Detours
	(
	_In_	PVOID	*Real_api,							// The real API
	_In_	PVOID	My_api,								// My Detoured API
	_In_	PCCH	Api_name							// API name
	)

//
// DESCRIPTION:		
//
// ASSUMPTIONS:		User mode
//
// SIDE EFFECTS:
//
// RETURN VALUES:
//
//

{
NTSTATUS	status;


	TRACE_ENTER ();
	TRACE_INFO (TRACEAPI, "Un-detouring API %s", Api_name);

	//
	// Shutdown Detours
	//

	if (ERR (status = DetourDetach (Real_api, My_api)))
		{
		TRACE_WARN (TRACEAPI, "Detach failed: `%s', status = %!STATUS!", det_real_name (Api_name), status);
		}

	TRACE_EXIT ();
	}							// End det_detach


PCCH
det_real_name											// 
	(
	_In_	PCCH Name									// 
	)

//
// DESCRIPTION:		
//
// ASSUMPTIONS:		User mode
//
// SIDE EFFECTS:
//
// RETURN VALUES:
//
//

{
PCCH	name_ptr = Name;


	TRACE_ENTER ();

	//
	// Move to end of name.
	//

	while (*name_ptr)
		{
		name_ptr++;
		}

	//
	// Move back through A-Za-z0-9 names.
	//

	while (name_ptr > Name &&
		   ((name_ptr [-1] >= 'A' && name_ptr [-1] <= 'Z') ||
			(name_ptr [-1] >= 'a' && name_ptr [-1] <= 'z') ||
			(name_ptr [-1] >= '0' && name_ptr [-1] <= '9')))
		{
		name_ptr--;
		}

	TRACE_EXIT ();
	return name_ptr;
}							// End det_real_name


BOOL 
process_attach											// Called when this DLL is loaded into a process
	(
	_In_	HMODULE		Dll_hdl							// This DLL's module handle							
	)

//
// DESCRIPTION:		
//
// ASSUMPTIONS:		User mode
//
// SIDE EFFECTS:
//
// RETURN VALUES:
//
//

{
HANDLE		process_hdl = GetCurrentProcess ();

	//
	// Initialize WPP tracing
	//

	WPP_INIT_TRACING (0);
	TRACE_ALWAYS (TRACEAPI, "%s", TA_product_description);
	TRACE_ENTER ();

NTSTATUS	status;
BOOL				ret_val = TRUE;
ULONG		length;


	//
	// Get the name of the executable that loaded this DLL
	//

	if ((length = GetProcessImageFileName (process_hdl, TA_image_file_name_buff, ARRAYSIZE (TA_image_file_name_buff))) != 0)
		{

		//
		// Fill in the string descriptor
		//

		TA_image_file_name.Buffer = TA_image_file_name_buff;
		TA_image_file_name.Length = (USHORT) length * 2;
		TA_image_file_name.MaximumLength = ARRAYSIZE (TA_image_file_name_buff) * 2;
		}
	else
		{
		status = GetLastError ();
		TRACE_ERROR (TRACEAPI, "Error getting image file name, status = %!STATUS!", status);
		}

	TA_tls_indent = TlsAlloc ();
	TA_tls_thread = TlsAlloc ();

	//
	// Initialize TraceLogging
	//

	if (SUCCESS (status = TraceLoggingRegisterEx (TA_tlg, nullptr, nullptr)))
		{

		//
		// Send a message that we've been loaded
		//

		TraceLoggingWrite (TA_tlg, "DLL-Attach", TraceLoggingOpcode (TL_OPC_DLL), TraceLoggingLevel (TRACE_LEVEL_VERBOSE),
			TraceLoggingKeyword (TL_KW_DLL), TraceLoggingDescription ("DLL attached to process"),
			TraceLoggingString (TA_product_description, "DLL description"),
			TraceLoggingUnicodeString (&TA_image_file_name,  "Image file name")
			);

		//
		// Replace the pointers to the API we are tracing
		//

		if (!SUCCESS (status = attach_detours ()))
			{
			TRACE_ERROR (TRACEAPI, "Error attaching Detour, status = %!STATUS!", status);
			}

		thread_attach (Dll_hdl);
		}
	else
		{
		TRACE_ERROR (TRACEAPI, "Error registering with TraceLogging, status = %!STATUS!", status);
		ret_val = FALSE;
		}

	TRACE_EXIT ();
	return ret_val;
}							// End process_attach

BOOL
process_detach											// Called when this DLL is unloaded from a process
	(
	_In_	HMODULE		Dll_hdl							// This DLL's module handle							
	)

//
// DESCRIPTION:		
//
// ASSUMPTIONS:		User mode
//
// SIDE EFFECTS:
//
// RETURN VALUES:
//
//

{
NTSTATUS	status;


	TRACE_ENTER ();

	TraceLoggingWrite (TA_tlg, "DLL-Detach", TraceLoggingOpcode (TL_OPC_DLL), TraceLoggingLevel (TRACE_LEVEL_VERBOSE),
		TraceLoggingKeyword (TL_KW_DLL), TraceLoggingDescription ("DLL detached from process"),
		TraceLoggingUnicodeString (&TA_image_file_name, "Image file name")
		);

	thread_detach (Dll_hdl);

	if (!SUCCESS (status = detach_detours ()))
		{
		TRACE_ERROR (TRACEAPI, "Error detaching Detour, status = %!STATUS!", status);
		}

	if (TA_tls_indent >= 0) 
		{
		TlsFree (TA_tls_indent);
		}

	if (TA_tls_thread >= 0) 
		{
		TlsFree (TA_tls_thread);
		}

	//
	// Disconnect from TraceLogging
	//

	TraceLoggingUnregister (TA_tlg);

	//
	// Close WPP tracing
	//

	TRACE_EXIT ();
	WPP_CLEANUP ();
	return TRUE;
}							// End process_detach


BOOL
thread_attach											// Called when the process creates a new thread
	(
	_In_	HMODULE		Dll_hdl							// This DLL's module handle							
	)

//
// DESCRIPTION:		
//
// ASSUMPTIONS:		User mode
//
// SIDE EFFECTS:
//
// RETURN VALUES:
//
//

{
LONG num_thread;


	UNREFERENCED_PARAMETER (Dll_hdl);

	TRACE_ENTER ();

	if (TA_tls_indent >= 0) 
		{
		TlsSetValue (TA_tls_indent, (PVOID) 0);
		}

	if (TA_tls_thread >= 0) 
		{
		num_thread = InterlockedIncrement (&TA_thread_count);
		TlsSetValue (TA_tls_thread, (PVOID) (LONG_PTR) num_thread);
		}

	TRACE_EXIT ();
	return TRUE;
}							// End thread_attach


BOOL 
thread_detach											// Called when a thread is exiting cleanly
	(
	_In_	HMODULE		Dll_hdl							// This DLL's module handle							
	)

//
// DESCRIPTION:		
//
// ASSUMPTIONS:		User mode
//
// SIDE EFFECTS:
//
// RETURN VALUES:
//
//

{
	UNREFERENCED_PARAMETER (Dll_hdl);

	TRACE_ENTER ();

	if (TA_tls_indent >= 0)
		{
		TlsSetValue (TA_tls_indent, (PVOID) 0);
		}

	if (TA_tls_thread >= 0) 
		{
		TlsSetValue(TA_tls_thread, (PVOID) 0);
		}

	TRACE_EXIT ();
	return TRUE;
}							// End thread_detach



