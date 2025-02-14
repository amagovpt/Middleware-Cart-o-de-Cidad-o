/* ****************************************************************************

 * eID Middleware Project.
 * Copyright (C) 2008-2009 FedICT.
 * Copyright (C) 2019 Caixa Magica Software.
 * Copyright (C) 2011 Vasco Silva - <vasco.silva@caixamagica.pt>
 * Copyright (C) 2012, 2016-2017 André Guerreiro - <aguerreiro1985@gmail.com>
 * Copyright (C) 2017 Luiz Lemos - <luiz.lemos@caixamagica.pt>
 * Copyright (C) 2021 Miguel Figueira - <miguelblcfigueira@gmail.com>
 *
 * This is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License version
 * 3.0 as published by the Free Software Foundation.
 *
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this software; if not, see
 * http://www.gnu.org/licenses/.

**************************************************************************** */
/********************************************************************************
********************************************************************************/
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include "errno.h"

#include "../dialogs.h"
#include "../langUtil.h"

#include "SharedMem.h"
#include <map>

#include "Log.h"
#include "Util.h"
#include "MWException.h"
#include "eidErrors.h"
#include "Config.h"
#include "Thread.h"
#include "prefix.h"

using namespace eIDMW;

std::map<unsigned long, DlgRunningProc *> dlgPinPadInfoCollector;
unsigned long dlgPinPadInfoCollectorIndex = 0;

std::map<unsigned long, DlgRunningProc *> dlgCMDMsgCollector;
unsigned long dlgCMDMsgCollectorIndex = 0;

std::string csServerName = "pteiddialogsQTsrv";

bool bRandInitialized = false;

static bool g_bSystemCallsFail = false;

/************************
 *       DIALOGS
 ************************/

// TODO: Add Keypad possibility in DlgAskPin(s)
DLGS_EXPORT DlgRet eIDMW::DlgAskPin(DlgPinOperation operation, DlgPinUsage usage, const wchar_t *wsPinName,
									DlgPinInfo pinInfo, wchar_t *wsPin, unsigned long ulPinBufferLen,
									void *wndGeometry) {
	DlgRet lRet = DLG_CANCEL;

	DlgAskPINArguments *oData;
	SharedMem oShMemory;
	std::string csReadableFilePath;

	try {
		csReadableFilePath = CreateRandomFile();

		// creating the shared memory segment
		// attach oData
		oShMemory.Attach(sizeof(DlgAskPINArguments), csReadableFilePath.c_str(), (void **)&oData);

		// collect the arguments into the struct placed
		// on the shared memory segment
		oData->operation = operation;
		oData->usage = usage;
		wcscpy_s(oData->pinName, sizeof(oData->pinName) / sizeof(wchar_t), wsPinName);
		oData->pinInfo = pinInfo;
		wcscpy_s(oData->pin, sizeof(oData->pin) / sizeof(wchar_t), wsPin);

		CallQTServer(DLG_ASK_PIN, csReadableFilePath.c_str(), wndGeometry);
		lRet = oData->returnValue;

		if (lRet == DLG_OK) {
			wcscpy_s(wsPin, ulPinBufferLen, oData->pin);
		}

		// detach from the segment
		oShMemory.Detach(oData);

		// delete the random file
		DeleteFile(csReadableFilePath.c_str());
	} catch (...) {

		// detach from the segment
		oShMemory.Detach(oData);

		// delete the random file
		DeleteFile(csReadableFilePath.c_str());

		return DLG_ERR;
	}
	return lRet;
}

DLGS_EXPORT DlgRet eIDMW::DlgAskPins(DlgPinOperation operation, DlgPinUsage usage, const wchar_t *wsPinName,
									 DlgPinInfo pin1Info, wchar_t *wsPin1, unsigned long ulPin1BufferLen,
									 DlgPinInfo pin2Info, wchar_t *wsPin2, unsigned long ulPin2BufferLen,
									 void *wndGeometry) {

	DlgRet lRet = DLG_CANCEL;

	DlgAskPINsArguments *oData;
	SharedMem oShMemory;
	std::string csReadableFilePath;

	try {
		csReadableFilePath = CreateRandomFile();

		// creating the shared memory segment
		// attach oData
		oShMemory.Attach(sizeof(DlgAskPINsArguments), csReadableFilePath.c_str(), (void **)&oData);

		// collect the arguments into the struct placed
		// on the shared memory segment
		oData->operation = operation;
		oData->usage = usage;
		wcscpy_s(oData->pinName, sizeof(oData->pinName) / sizeof(wchar_t), wsPinName);
		oData->pin1Info = pin1Info;
		oData->pin2Info = pin2Info;
		wcscpy_s(oData->pin1, sizeof(oData->pin1) / sizeof(wchar_t), wsPin1);
		wcscpy_s(oData->pin2, sizeof(oData->pin2) / sizeof(wchar_t), wsPin2);

		CallQTServer(DLG_ASK_PINS, csReadableFilePath.c_str(), wndGeometry);
		lRet = oData->returnValue;

		if (lRet == DLG_OK) {
			wcscpy_s(wsPin1, ulPin1BufferLen, oData->pin1);
			wcscpy_s(wsPin2, ulPin2BufferLen, oData->pin2);
		}

		// detach from the segment
		oShMemory.Detach(oData);

		// delete the random file
		DeleteFile(csReadableFilePath.c_str());
	} catch (...) {
		// detach from the segment
		oShMemory.Detach(oData);

		// delete the random file
		DeleteFile(csReadableFilePath.c_str());

		return DLG_ERR;
	}
	return lRet;
}

DLGS_EXPORT DlgRet eIDMW::DlgBadPin(DlgPinUsage usage, const wchar_t *wsPinName, unsigned long ulRemainingTries,
									void *wndGeometry) {
	DlgRet lRet = DLG_CANCEL;

	DlgBadPinArguments *oData;
	SharedMem oShMemory;
	std::string csReadableFilePath;

	try {

		csReadableFilePath = CreateRandomFile();

		// creating the shared memory segment
		// attach oData

		oShMemory.Attach(sizeof(DlgBadPinArguments), csReadableFilePath.c_str(), (void **)&oData);

		// collect the arguments into the struct placed
		// on the shared memory segment
		oData->usage = usage;
		wcscpy_s(oData->pinName, sizeof(oData->pinName) / sizeof(wchar_t), wsPinName);
		oData->ulRemainingTries = ulRemainingTries;

		CallQTServer(DLG_BAD_PIN, csReadableFilePath.c_str(), wndGeometry);
		lRet = oData->returnValue;

		// detach from the segment
		oShMemory.Detach(oData);

		// delete the random file
		DeleteFile(csReadableFilePath.c_str());
	} catch (...) {
		// detach from the segment
		oShMemory.Detach(oData);

		// delete the random file
		DeleteFile(csReadableFilePath.c_str());

		return DLG_ERR;
	}
	return lRet;
}

DLGS_EXPORT DlgRet eIDMW::DlgDisplayPinpadInfo(DlgPinOperation operation, const wchar_t *wsReader, DlgPinUsage usage,
											   const wchar_t *wsPinName, const wchar_t *wsMessage,
											   unsigned long *pulHandle, void *wndGeometry) {
	DlgRet lRet = DLG_CANCEL;

	DlgDisplayPinpadInfoArguments *oData;
	SharedMem oShMemory;
	std::string csReadableFilePath;

	try {
		MWLOG(LEV_DEBUG, MOD_DLG, L"  eIDMW::DlgDisplayPinpadInfo called");
		csReadableFilePath = CreateRandomFile();

		oShMemory.Attach(sizeof(DlgDisplayPinpadInfoArguments), csReadableFilePath.c_str(), (void **)&oData);

		// collect the arguments into the struct placed
		// on the shared memory segment

		oData->operation = operation;
		wcscpy_s(oData->reader, sizeof(oData->reader) / sizeof(wchar_t), wsReader);
		oData->usage = usage;
		wcscpy_s(oData->pinName, sizeof(oData->pinName) / sizeof(wchar_t), wsPinName);
		wcscpy_s(oData->message, sizeof(oData->message) / sizeof(wchar_t), wsMessage);
		oData->infoCollectorIndex = ++dlgPinPadInfoCollectorIndex;

		CallQTServer(DLG_DISPLAY_PINPAD_INFO, csReadableFilePath.c_str(), wndGeometry);
		lRet = oData->returnValue;

		if (lRet != DLG_OK) {
			throw CMWEXCEPTION(EIDMW_ERR_UNKNOWN);
		}

		// for the killing need to store:
		// - the shared memory area to be released (unique with the filename?)
		// - the child process ID
		// - the handle (because the user will use it)

		DlgRunningProc *ptRunningProc = new DlgRunningProc();
		ptRunningProc->iSharedMemSegmentID = oShMemory.getID();
		ptRunningProc->csRandomFilename = csReadableFilePath;

		ptRunningProc->tRunningProcess = oData->tRunningProcess;

		dlgPinPadInfoCollector[dlgPinPadInfoCollectorIndex] = ptRunningProc;

		if (pulHandle)
			*pulHandle = dlgPinPadInfoCollectorIndex;

		oShMemory.Detach(oData);
	} catch (...) {

		oShMemory.Detach(oData);

		// delete the random file
		DeleteFile(csReadableFilePath.c_str());

		MWLOG(LEV_ERROR, MOD_DLG, L"  eIDMW::DlgDisplayPinpadInfo failed");

		return DLG_ERR;
	}
	return lRet;
}

DLGS_EXPORT void eIDMW::DlgClosePinpadInfo(unsigned long ulHandle) {
	// check if we have this handle
	std::map<unsigned long, DlgRunningProc *>::iterator pIt = dlgPinPadInfoCollector.find(ulHandle);

	if (pIt != dlgPinPadInfoCollector.end()) {

		// check if the process is still running
		// and send SIGTERM if so
		if (!kill(pIt->second->tRunningProcess, 0)) {

			MWLOG(LEV_DEBUG, MOD_DLG, L"  eIDMW::DlgClosePinpadInfo :  sending kill signal to process %d",
				  pIt->second->tRunningProcess);

			if (kill(pIt->second->tRunningProcess, SIGINT)) {

				MWLOG(LEV_ERROR, MOD_DLG, L"  eIDMW::DlgClosePinpadInfo sent signal SIGINT to proc %d Error: %s ",
					  pIt->second->tRunningProcess, strerror(errno));

				throw CMWEXCEPTION(EIDMW_ERR_UNKNOWN);
			}

		} else {
			MWLOG(LEV_ERROR, MOD_DLG, L"  eIDMW::DlgClosePinpadInfo sent signal 0 to proc %d : Error %s ",
				  pIt->second->tRunningProcess, strerror(errno));
			throw CMWEXCEPTION(EIDMW_ERR_UNKNOWN);
		}

		// delete the random file
		DeleteFile(pIt->second->csRandomFilename.c_str());

		// delete the map entry

		delete pIt->second;
		pIt->second = NULL;
		dlgPinPadInfoCollector.erase(pIt);

		// memory is cleaned up in the child process
	}
}

DLGS_EXPORT void eIDMW::DlgCloseAllPinpadInfo() {

	// check if we have this handle
	for (std::map<unsigned long, DlgRunningProc *>::iterator pIt = dlgPinPadInfoCollector.begin();
		 pIt != dlgPinPadInfoCollector.end(); ++pIt) {

		// check if the process is still running
		// and send SIGTERM if so
		if (!kill(pIt->second->tRunningProcess, 0)) {

			MWLOG(LEV_INFO, MOD_DLG, L"  eIDMW::DlgCloseAllPinpadInfo :  sending kill signal to process %d\n",
				  pIt->second->tRunningProcess);

			if (kill(pIt->second->tRunningProcess, SIGINT)) {
				MWLOG(LEV_ERROR, MOD_DLG, L"  eIDMW::DlgCloseAllPinpadInfo sent signal SIGINT to proc %d : %s ",
					  pIt->second->tRunningProcess, strerror(errno));
				throw CMWEXCEPTION(EIDMW_ERR_UNKNOWN);
			}
		} else {
			MWLOG(LEV_ERROR, MOD_DLG, L"  eIDMW::DlgCloseAllPinpadInfo sent signal 0 to proc %d : %s ",
				  pIt->second->tRunningProcess, strerror(errno));
			throw CMWEXCEPTION(EIDMW_ERR_UNKNOWN);
		}

		// delete the random file
		DeleteFile(pIt->second->csRandomFilename.c_str());

		delete pIt->second;
		pIt->second = NULL;

		// memory is cleaned up in the child process
	}
	// delete the map
	dlgPinPadInfoCollector.clear();
}

DLGS_EXPORT DlgRet eIDMW::DlgAskInputCMD(DlgCmdOperation operation, bool isValidateOtp, wchar_t *csOutCode,
										 unsigned long ulOutCodeBufferLen, wchar_t *csInOutId, unsigned long ulOutIdLen,
										 const wchar_t *csUserName, unsigned long ulUserNameBufferLen,
										 std::function<void(void)> *fSendSmsCallback) {
	MWLOG(LEV_DEBUG, MOD_DLG, L"  eIDMW::DlgAskInputCMD called");
	DlgRet lRet = DLG_CANCEL;

	DlgAskInputCMDArguments *oData;
	SharedMem oShMemory;
	std::string csReadableFilePath;

	try {
		csReadableFilePath = CreateRandomFile();

		// creating the shared memory segment
		// attach oData
		oShMemory.Attach(sizeof(DlgAskInputCMDArguments), csReadableFilePath.c_str(), (void **)&oData);

		// collect the arguments into the struct placed
		// on the shared memory segment
		oData->isValidateOtp = isValidateOtp;
		oData->operation = operation;
		oData->askForId = ulOutIdLen != 0;

		if (isValidateOtp) {
			wcsncpy(oData->inOutId, csInOutId, sizeof(oData->inOutId) / sizeof(wchar_t));
		} else {
			// Cached mobile number for CMD PIN dialog
			wcsncpy(oData->inOutId, csInOutId, ulOutIdLen);
		}

		CallQTServer(DLG_ASK_CMD_INPUT, csReadableFilePath.c_str(), NULL);
		lRet = oData->returnValue;

		/* If the callback button to send the sms was pressed, call callback and reopen the dialog.
		  callbackWasCalled in oData is set to true to disable the button. */
		if (oData->returnValue == DLG_CALLBACK) {
			(*fSendSmsCallback)();
			oData->callbackWasCalled = true;
			CallQTServer(DLG_ASK_CMD_INPUT, csReadableFilePath.c_str(), NULL);
			lRet = oData->returnValue;
		}

		if (lRet == DLG_OK) {
			if (!isValidateOtp) {
				wcscpy_s(csInOutId, ulOutIdLen, oData->inOutId);
			}

			wcscpy_s(csOutCode, ulOutCodeBufferLen, oData->Code);
		}

		// detach from the segment
		oShMemory.Detach(oData);

		// delete the random file
		DeleteFile(csReadableFilePath.c_str());
	} catch (...) {
		// detach from the segment
		oShMemory.Detach(oData);

		// delete the random file
		DeleteFile(csReadableFilePath.c_str());

		return DLG_ERR;
	}
	return lRet;
}

DLGS_EXPORT DlgRet eIDMW::DlgPickDevice(DlgDevice *outDevice) {
	MWLOG(LEV_DEBUG, MOD_DLG, L"  eIDMW::DlgPickDevice called");
	DlgRet lRet = DLG_CANCEL;

	DlgPickDeviceArguments *oData;
	SharedMem oShMemory;
	std::string csReadableFilePath;

	try {
		csReadableFilePath = CreateRandomFile();

		// creating the shared memory segment
		// attach oData
		oShMemory.Attach(sizeof(DlgPickDeviceArguments), csReadableFilePath.c_str(), (void **)&oData);

		CallQTServer(DLG_PICK_DEVICE, csReadableFilePath.c_str(), NULL);
		lRet = oData->returnValue;

		if (lRet == DLG_OK) {
			*outDevice = oData->outDevice;
		}

		// detach from the segment
		oShMemory.Detach(oData);

		// delete the random file
		DeleteFile(csReadableFilePath.c_str());
	} catch (...) {
		// detach from the segment
		oShMemory.Detach(oData);

		// delete the random file
		DeleteFile(csReadableFilePath.c_str());

		return DLG_ERR;
	}
	return lRet;
}

DLGS_EXPORT DlgRet eIDMW::DlgCMDMessage(DlgCmdOperation operation, DlgCmdMsgType type, bool isOtp,
										unsigned long *pulHandle) {
	const wchar_t *message = NULL;
	if (isOtp) {
		message = GETSTRING_DLG(SendingOtp);
	} else {
		message = GETSTRING_DLG(ConnectingWithServer);
	}
	return DlgCMDMessage(operation, type, message, pulHandle);
}

DLGS_EXPORT DlgRet eIDMW::DlgCMDMessage(DlgCmdOperation operation, DlgCmdMsgType type, const wchar_t *message,
										unsigned long *pulHandle) {
	MWLOG(LEV_DEBUG, MOD_DLG, L"  eIDMW::DlgCMDMessage called");
	DlgRet lRet = DLG_CANCEL;

	DlgCMDMessageArguments *oCmdMessageData;
	SharedMem oShMemory;
	std::string csReadableFilePath;

	try {
		csReadableFilePath = CreateRandomFile();

		// creating the shared memory segment
		// attach oCmdMessageData
		oShMemory.Attach(sizeof(DlgCMDMessageArguments), csReadableFilePath.c_str(), (void **)&oCmdMessageData);

		oCmdMessageData->type = type;
		oCmdMessageData->operation = operation;
		wcscpy_s(oCmdMessageData->message, sizeof(oCmdMessageData->message) / sizeof(wchar_t), message);
		oCmdMessageData->cmdMsgCollectorIndex = ++dlgCMDMsgCollectorIndex;

		CallQTServer(DLG_CMD_MSG, csReadableFilePath.c_str(), NULL);

		DlgRunningProc *ptRunningProc = new DlgRunningProc();
		ptRunningProc->iSharedMemSegmentID = oShMemory.getID();
		ptRunningProc->csRandomFilename = csReadableFilePath;

		ptRunningProc->tRunningProcess = oCmdMessageData->tRunningProcess;

		dlgCMDMsgCollector[dlgCMDMsgCollectorIndex] = ptRunningProc;

		if (pulHandle)
			*pulHandle = dlgCMDMsgCollectorIndex;

		/* Wait for dialog process to die. It is not direct child so waitpid does not work.
		Timeout after 1 minute.*/
		for (size_t i = 0; i < 600; i++) {
			CThread::SleepMillisecs(100);
			// Check if process is running. If no (kill fails), break;
			if (kill(ptRunningProc->tRunningProcess, 0)) {
				break;
			}
		}

		// delete the map entry

		delete ptRunningProc;
		dlgCMDMsgCollector[dlgCMDMsgCollectorIndex] = NULL;
		dlgCMDMsgCollector.erase(dlgCMDMsgCollectorIndex);

		lRet = oCmdMessageData->returnValue;

		// detach from the segment
		oShMemory.Detach(oCmdMessageData);
		oCmdMessageData = NULL;

		// delete the random file
		// DeleteFile(csReadableFilePath.c_str());
		if (access(csReadableFilePath.c_str(), F_OK) != -1) {
			// delete the random file
			DeleteFile(csReadableFilePath.c_str());
		}

	} catch (...) {
		// detach from the segment
		oShMemory.Detach(oCmdMessageData);
		oCmdMessageData = NULL;

		// delete the random file
		DeleteFile(csReadableFilePath.c_str());

		MWLOG(LEV_ERROR, MOD_DLG, L"  eIDMW::DlgCMDMessage failed");

		return DLG_ERR;
	}

	return lRet;
}

DLGS_EXPORT void eIDMW::DlgCloseCMDMessage(unsigned long ulHandle) {
	MWLOG(LEV_DEBUG, MOD_DLG, L"DlgCloseCMDMessage() called: handle=%lu", ulHandle);
	// check if we have this handle
	std::map<unsigned long, DlgRunningProc *>::iterator pIt = dlgCMDMsgCollector.find(ulHandle);

	if (pIt != dlgCMDMsgCollector.end()) {

		// check if the process is still running
		// and send SIGTERM if so
		if (!kill(pIt->second->tRunningProcess, 0)) {

			MWLOG(LEV_DEBUG, MOD_DLG, L"  eIDMW::DlgCloseCMDMessage :  sending kill signal to process %d",
				  pIt->second->tRunningProcess);

			if (kill(pIt->second->tRunningProcess, SIGINT)) {

				MWLOG(LEV_ERROR, MOD_DLG, L"  eIDMW::DlgCloseCMDMessage sent signal SIGINT to proc %d Error: %s ",
					  pIt->second->tRunningProcess, strerror(errno));

				throw CMWEXCEPTION(EIDMW_ERR_UNKNOWN);
			}

		} else {
			MWLOG(LEV_ERROR, MOD_DLG, L"  eIDMW::DlgCloseCMDMessage sent signal 0 to proc %d : Error %s ",
				  pIt->second->tRunningProcess, strerror(errno));
			throw CMWEXCEPTION(EIDMW_ERR_UNKNOWN);
		}

		// memory is cleaned up in the child process
	}
}

/***************************
 *       Helper Functions
 ***************************/

void eIDMW::InitializeRand() {
	if (bRandInitialized)
		return;
	srand(time(NULL));
	bRandInitialized = true;
	return;
}

std::string eIDMW::RandomFileName() {

	InitializeRand();

	// start the filename with a dot, so that it is not visible with a normal 'ls'
	std::string randomFileName = "/tmp/.file_";
	char rndmString[13];
	sprintf(rndmString, "%012d", rand());

	randomFileName += rndmString;

	return randomFileName;
}

std::string eIDMW::CreateRandomFile() {

	std::string csFilePath = RandomFileName();
	// create this file
	char csCommand[100];
	sprintf(csCommand, "touch %s", csFilePath.c_str());
	if (system(csCommand) != 0) {
		// If this lib is used by acroread, all system() calls
		// seems to return -1 for some reason, even if the
		// call was successfull.
		FILE *test = fopen(csFilePath.c_str(), "r");
		if (test) {
			fclose(test);
			g_bSystemCallsFail = true;
			MWLOG(LEV_WARN, MOD_DLG, L"  eIDMW::CreateRandomFile %s : %s (%d)", csFilePath.c_str(), strerror(errno),
				  errno);
		} else {
			MWLOG(LEV_ERROR, MOD_DLG, L"  eIDMW::CreateRandomFile %s : %s (%d)", csFilePath.c_str(), strerror(errno),
				  errno);
			throw CMWEXCEPTION(EIDMW_ERR_UNKNOWN);
		}
	}
	return csFilePath;
}

void eIDMW::DeleteFile(const char *csFilename) {
	char csCommand[100];
	sprintf(csCommand, " [ -e %s ] && rm %s", csFilename, csFilename);
	if (system(csCommand) != 0) {
		MWLOG(g_bSystemCallsFail ? LEV_WARN : LEV_ERROR, MOD_DLG, L"  eIDMW::DeleteFile %s : %s ", csFilename,
			  strerror(errno));
		// throw CMWEXCEPTION(EIDMW_ERR_SYSTEM);
	}
}

void eIDMW::CallQTServer(const DlgFunctionIndex index, const char *csFilename, void *wndGeometry) {
	char csCommand[150];
	Type_WndGeometry *pWndGeometry = (Type_WndGeometry *)wndGeometry;

	std::string csServerPath = STRINGIFY(EIDMW_PREFIX) "/bin/";
#ifdef __APPLE__
	csServerPath += "pteiddialogsQTsrv.app/Contents/MacOS/";
#endif

	sprintf(csCommand, "%s/%s %i %s", csServerPath.c_str(), csServerName.c_str(), index, csFilename);

	if ((pWndGeometry != NULL) && (pWndGeometry->x >= 0) && (pWndGeometry->y >= 0) && (pWndGeometry->width >= 0) &&
		(pWndGeometry->height >= 0)) {
		int len = strlen(csCommand);
		sprintf(&csCommand[len], " %i %i %i %i", pWndGeometry->x, pWndGeometry->y, pWndGeometry->width,
				pWndGeometry->height);
	}

	int code = system(csCommand);
	if (code != 0) {
		MWLOG(g_bSystemCallsFail ? LEV_WARN : LEV_ERROR, MOD_DLG, L"  eIDMW::CallQTServer %i %s : %s ", index,
			  csFilename, strerror(errno));
		if (!g_bSystemCallsFail)
			throw CMWEXCEPTION(EIDMW_ERR_UNKNOWN);
	}
	return;
}

bool eIDMW::getWndCenterPos(Type_WndGeometry *pWndGeometry, int desktop_width, int desktop_height, int wnd_width,
							int wnd_height, Type_WndGeometry *outWndGeometry) {

	if (outWndGeometry == NULL || pWndGeometry == NULL)
		return false;
	if ((desktop_width < 0) || (desktop_height < 0) || (wnd_width < 0) || (wnd_height < 0))
		return false;

	memset(outWndGeometry, -1, sizeof(Type_WndGeometry));

	outWndGeometry->x = pWndGeometry->x + ((pWndGeometry->width - wnd_width) / 2);
	outWndGeometry->y = pWndGeometry->y + ((pWndGeometry->height - wnd_height) / 2);

	if (outWndGeometry->x < 0 || outWndGeometry->x > desktop_width)
		return false;
	if (outWndGeometry->y < 0 || outWndGeometry->y > desktop_height)
		return false;

	return true;
}
