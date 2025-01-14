/* ****************************************************************************

 * eID Middleware Project.
 * Copyright (C) 2008-2009 FedICT.
 * Copyright (C) 2019 Caixa Magica Software.
 * Copyright (C) 2011 Vasco Silva - <vasco.silva@caixamagica.pt>
 * Copyright (C) 2011-2014, 2016-2018 André Guerreiro - <aguerreiro1985@gmail.com>
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
/****************************************************************************************************/

#include "globmdrv.h"
#include "log.h"
#include "smartcard.h"
#include "externalpinui.h"
#include "util.h"
#include "cache.h"

#include <commctrl.h>
#include "winerror.h"
/****************************************************************************************************/

#define PTEID_MIN_USER_PIN_LEN 4
#define PTEID_MAX_USER_PIN_LEN 8

/****************************************************************************************************/

#define WHERE "PteidDelayAndRecover"
void PteidDelayAndRecover(PCARD_DATA pCardData, BYTE SW1, BYTE SW2, DWORD dwReturn) {
	if ((dwReturn == SCARD_E_COMM_DATA_LOST) || (dwReturn == SCARD_E_NOT_TRANSACTED)) {
		DWORD ap = 0;
		int i = 0;

		LogTrace(LOGTYPE_WARNING, WHERE, "Card is confused, trying to recover...");

		for (i = 0; (i < 10) && (dwReturn != SCARD_S_SUCCESS); i++) {
			if (i != 0)
				Sleep(1000);

			dwReturn = SCardReconnect(pCardData->hScard, SCARD_SHARE_SHARED, SCARD_PROTOCOL_T0, SCARD_RESET_CARD, &ap);
			if (dwReturn != SCARD_S_SUCCESS) {
				LogTrace(LOGTYPE_DEBUG, WHERE, "  [%d] SCardReconnect errorcode: [0x%02X]", i, dwReturn);
				continue;
			}
			// transaction is lost after an SCardReconnect()
			dwReturn = SCardBeginTransaction(pCardData->hScard);
			if (dwReturn != SCARD_S_SUCCESS) {
				LogTrace(LOGTYPE_DEBUG, WHERE, "  [%d] SCardBeginTransaction errorcode: [0x%02X]", i, dwReturn);
				continue;
			}
			dwReturn = PteidSelectApplet(pCardData);
			if (dwReturn != SCARD_S_SUCCESS) {
				LogTrace(LOGTYPE_DEBUG, WHERE, "  [%d] SCardSelectApplet errorcode: [0x%02X]", i, dwReturn);
				continue;
			}

			LogTrace(LOGTYPE_INFO, WHERE, "  Card recovered in loop %d", i);
		}
		if (i >= 10) {
			LogTrace(LOGTYPE_ERROR, WHERE, "SCardTransmit errorcode: [0x%02X], Failed to recover", dwReturn);
		}
	}
	if (((SW1 == 0x90) && (SW2 == 0x00)) || (SW1 == 0x61) || (SW1 == 0x6c)) {
		; // no error received, no sleep needed
	} else {
		Sleep(25);
	}
}
#undef WHERE

void GemPCLoadStrings(SCARDHANDLE hCard, DWORD pin_id) {
	/*The Following Blob contains the Portuguese strings to show on the Pinpad Display:
			PIN Autent.?
			Novo PIN?
			Conf. novo PIN
			OK
			PIN falhou
			Tempo expirou
			* tentiv. restam
			Introduza cartao
			Erro cartao
			PIN bloqueado
		*/
	char stringTable[] =
		"\xB2\xA0\x00\x4D\x4C\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x4E\x6F\x76"
		"\x6F\x20\x50\x49\x4E\x3F\x20\x20\x20\x20\x20\x20\x20\x43\x6F\x6E\x66\x2E\x20\x6E\x6F\x76\x6F"
		"\x20\x50\x49\x4E\x20\x20\x50\x49\x4E\x20\x4F\x4B\x2E\x20\x20\x20\x20\x20\x20\x20\x20\x20\x50"
		"\x49\x4E\x20\x66\x61\x6C\x68\x6F\x75\x20\x20\x20\x20\x20\x20\x54\x65\x6D\x70\x6F\x20\x65\x78"
		"\x70\x69\x72\x6F\x75\x20\x20\x20\x2A\x20\x74\x65\x6E\x74\x69\x76\x2E\x20\x72\x65\x73\x74\x61\x6D"
		"\x49\x6E\x74\x72\x6F\x64\x75\x7A\x61\x20\x63\x61\x72\x74\x61\x6F\x45\x72\x72\x6F\x20\x63\x61\x72"
		"\x74\x61\x6F\x20\x20\x20\x20\x20\x50\x49\x4E\x20\x62\x6C\x6F\x71\x75\x65\x61\x64\x6F\x20\x20\x20";

	DWORD ioctl = 0x00312000;
	char recvBuf[200];
	DWORD rv = 0;
	unsigned int STRING_LEN = 16;
	DWORD recvlen = sizeof(recvBuf);

	switch (pin_id) {
	case 0:
		memcpy(&stringTable[5], "PIN Autent.?    ", STRING_LEN);
		break;
	case 1:
		memcpy(&stringTable[5], "PIN Assinatura? ", STRING_LEN);
		break;
	}

	rv = SCardControl(hCard, ioctl, stringTable, sizeof(stringTable) - 1, recvBuf, recvlen, &recvlen);

	if (rv == SCARD_S_SUCCESS) {
		LogTrace(LOGTYPE_INFO, "GemPCLoadStrings()", "Strings Loaded successfully");
	} else
		LogTrace(LOGTYPE_INFO, "GemPCLoadStrings()", "Error in LoadStrings: SCardControl() returned: %08x",
				 (unsigned int)rv);
}

/****************************************************************************************************/

#define WHERE "PteidAuthenticateExternal"

// Helper macros to ease the transition to 3 arg version of swprintf
#define WRITE_MAIN_INSTRUCTION(MESSAGE) swprintf(wchMainInstruction, 100, MESSAGE)
#define WRITE_ERROR_MESSAGE(MESSAGE) swprintf(wchErrorMessage, 500, MESSAGE)

DWORD PteidAuthenticateExternal(PCARD_DATA pCardData, PDWORD pcAttemptsRemaining, BOOL bSilent, DWORD pin_id) {
	DWORD dwReturn = 0;
	SCARD_IO_REQUEST ioSendPci = {1, sizeof(SCARD_IO_REQUEST)};
	SCARD_IO_REQUEST ioRecvPci = {1, sizeof(SCARD_IO_REQUEST)};

	PIN_VERIFY_STRUCTURE verifyCommand;

	unsigned int uiCmdLg = 0;
	unsigned int pin_ref = 0;
	unsigned int is_gempc = 0;
	unsigned char recvbuf[256];
	unsigned char szReaderName[256];
	DWORD reader_length = sizeof(szReaderName);
	unsigned long recvlen = sizeof(recvbuf);
	BYTE SW1, SW2;
	int i = 0;
	int offset = 0;
	DWORD dwDataLen;
	BOOL bRetry = TRUE;
	int nButton;
	LONG status_rv;

	EXTERNAL_PIN_INFORMATION externalPinInfo;
	HANDLE DialogThreadHandle;

	LogTrace(LOGTYPE_INFO, WHERE, "Enter API...");

	/********************/
	/* Check Parameters */
	/********************/
	if (pCardData == NULL) {
		LogTrace(LOGTYPE_ERROR, WHERE, "Invalid parameter [pCardData]");
		CLEANUP(SCARD_E_INVALID_PARAMETER);
	}

	/*********************/
	/* External PIN Info */
	/*********************/
	externalPinInfo.hCardHandle = pCardData->hScard;
	CCIDgetFeatures(&(externalPinInfo.features), externalPinInfo.hCardHandle);

	/*********************/
	/* Get Parent Window */
	/*********************/
	dwReturn = CardGetProperty(pCardData, CP_PARENT_WINDOW, (PBYTE) & (externalPinInfo.hwndParentWindow),
							   sizeof(externalPinInfo.hwndParentWindow), &dwDataLen, 0);
	if (dwReturn != 0) {
		LogTrace(LOGTYPE_ERROR, WHERE, "CardGetProperty Failed: %02X", dwReturn);
		externalPinInfo.hwndParentWindow = NULL;
	}

	/*********************/
	/* Get Pin Context String */
	/*********************/
	dwReturn = CardGetProperty(pCardData, CP_PIN_CONTEXT_STRING, (PBYTE)externalPinInfo.lpstrPinContextString,
							   sizeof(externalPinInfo.lpstrPinContextString), &dwDataLen, 0);
	if (dwReturn != 0) {
		LogTrace(LOGTYPE_ERROR, WHERE, "CardGetProperty Failed: %02X", dwReturn);
		wcscpy(externalPinInfo.lpstrPinContextString, L"");
	}

	/**********/
	/* Log On */
	/**********/

	if (card_type == IAS_CARD && pin_id == 0)
		pin_ref = 0x01;
	else
		pin_ref = 0x81 + pin_id;

	// SCardStatus to get the reader name
	status_rv = SCardStatus(pCardData->hScard, szReaderName, &reader_length, NULL, NULL, NULL, NULL);

	if (strstr(szReaderName, "GemPC Pinpad") != 0 || strstr(szReaderName, "GemPCPinpad") != 0) {
		createVerifyCommandGemPC(&verifyCommand, pin_ref);
		is_gempc = 1;
	} else if (strstr(szReaderName, "ACR83U") != 0) {
		createVerifyCommandACR83(&verifyCommand, pin_ref);
	} else
		createVerifyCommand(&verifyCommand, pin_ref);

	uiCmdLg = sizeof(verifyCommand);
	recvlen = sizeof(recvbuf);

	PteidSelectApplet(pCardData);

	while (bRetry) {
		bRetry = FALSE;
		nButton = -1;

		// We introduce a short sleep before starting the PIN VERIFY procedure
		// Reason: we do this for users with a combined keyboard/secure PIN pad smartcard reader
		//   "enter" key far right on the keyboard ==  "OK" button of the PIN pad
		//   Problem: key becomes PIN-pad button before key is released. Result: the keyup event is not sent.
		//   This sleep gives the user some time to release the Enter key.

		Sleep(100);

		if (externalPinInfo.features.VERIFY_PIN_DIRECT != 0) {
			externalPinInfo.iPinCharacters = 0;
			externalPinInfo.cardState = CS_PINENTRY;
			// show dialog
			if (!bSilent)
				DialogThreadHandle = CreateThread(NULL, 0, DialogThreadPinEntry, &externalPinInfo, 0, NULL);

			LogTrace(LOGTYPE_INFO, WHERE, "Running SCardControl with ioctl=%08x",
					 externalPinInfo.features.VERIFY_PIN_DIRECT);
			LogTrace(LOGTYPE_INFO, WHERE, "PIN_VERIFY_STRUCT: ");
			LogDumpHex(uiCmdLg, (unsigned char *)&verifyCommand);

			if (is_gempc == 1)
				GemPCLoadStrings(pCardData->hScard, pin_id);

			dwReturn = SCardControl(pCardData->hScard, externalPinInfo.features.VERIFY_PIN_DIRECT, &verifyCommand,
									uiCmdLg, recvbuf, recvlen, &recvlen);
			SW1 = recvbuf[recvlen - 2];
			SW2 = recvbuf[recvlen - 1];

			externalPinInfo.cardState = CS_PINENTERED;
			if (dwReturn != SCARD_S_SUCCESS) {
				LogTrace(LOGTYPE_ERROR, WHERE, "SCardControl errorcode: [0x%02X]", dwReturn);
				CLEANUP(dwReturn);
			} else {
				if ((SW1 != 0x90) || (SW2 != 0x00)) {

					LogTrace(LOGTYPE_ERROR, WHERE, "CardAuthenticateExternal Failed: [0x%02X][0x%02X]", SW1, SW2);

					if (((SW1 == 0x63) && ((SW2 & 0xF0) == 0xC0))) {
						if (pcAttemptsRemaining != NULL) {
							/* -1: Don't support returning the count of remaining authentication attempts */
							*pcAttemptsRemaining = (SW2 & 0x0F);
						}
					} else if ((SW1 == 0x69) && (SW2 == 0x83 || SW2 == 0x84)) {
						dwReturn = SCARD_W_CHV_BLOCKED;
						LogTrace(LOGTYPE_ERROR, WHERE, "PIN with ID %d is blocked, watch out!!", (int)pin_id);
					}
				} else
					LogTrace(LOGTYPE_INFO, WHERE, "Logged in via Pinpad Reader");
			}
		}
	}

cleanup:

	LogTrace(LOGTYPE_INFO, WHERE, "Exit API...");
	return (dwReturn);
}
#undef WHERE

BYTE getGemaltoAlgoID(DWORD hash_len) {
	switch (hash_len) {
	case SHA1_LEN:
		return 0x12;
	case SHA256_LEN:
		return 0x42;
	case SHA384_LEN:
		return 0x52;
	case SHA512_LEN:
		return 0x62;
	default:
		return 0x02;
	}
}

BYTE getIASv5AlgoId(DWORD hash_len) {
	switch (hash_len) {
	case SHA1_LEN:
		return 0x14;
	case SHA512_LEN:
		return 0x64;
	case SHA384_LEN:
		return 0x54;
	case SHA256_LEN:
		return 0x44;
	default:
		return 0x02;
	}
}

/****************************************************************************************************/

BOOL checkStatusCode(const char *context, DWORD dwReturn, BYTE SW1, BYTE SW2) {

	if (dwReturn != SCARD_S_SUCCESS) {
		LogTrace(LOGTYPE_ERROR, context, "SCardTransmit errorcode: [0x%02X]", dwReturn);
		return FALSE;
	}
	if ((SW1 != 0x90) || (SW2 != 0x00)) {
		LogTrace(LOGTYPE_ERROR, context, "Card returned SW12: [0x%02X][0x%02X]", SW1, SW2);
		return FALSE;
	}

	return TRUE;
}

#define SHA256_DIGESTINFO 1
#define SHA384_DIGESTINFO 2
#define SHA512_DIGESTINFO 3

static const unsigned char SHA256_AID[] = {0x30, 0x31, 0x30, 0x0d, 0x06, 0x09, 0x60, 0x86, 0x48, 0x01,
										   0x65, 0x03, 0x04, 0x02, 0x01, 0x05, 0x00, 0x04, 0x20};
static const unsigned char SHA384_AID[] = {0x30, 0x41, 0x30, 0x0d, 0x06, 0x09, 0x60, 0x86, 0x48, 0x01,
										   0x65, 0x03, 0x04, 0x02, 0x02, 0x05, 0x00, 0x04, 0x30};
static const unsigned char SHA512_AID[] = {0x30, 0x51, 0x30, 0x0d, 0x06, 0x09, 0x60, 0x86, 0x48, 0x01,
										   0x65, 0x03, 0x04, 0x02, 0x03, 0x05, 0x00, 0x04, 0x40};

/* Return one of the constants SHA256_PREFIX, SHA384_PREFIX, SHA512_PREFIX if the supplied byte array is in the expected
   ASN.1 DigestInfo structure for those types of hashes or the constant 0 if it doesn't match any known structure From
   RFC 8017 (PKCS#1 version 2.2): DigestInfo ::= SEQUENCE { digestAlgorithm AlgorithmIdentifier, digest OCTET STRING
			   }
*/
unsigned int matchDigestInfoPrefix(PBYTE hash, DWORD hash_len) {

	switch (hash_len) {
	case SHA256_LEN + sizeof(SHA256_AID):
		return memcmp(hash, SHA256_AID, sizeof(SHA256_AID)) == 0 ? SHA256_DIGESTINFO : 0;

	case SHA384_LEN + sizeof(SHA384_AID):
		return memcmp(hash, SHA384_AID, sizeof(SHA384_AID)) == 0 ? SHA384_DIGESTINFO : 0;

	case SHA512_LEN + sizeof(SHA512_AID):
		return memcmp(hash, SHA512_AID, sizeof(SHA512_AID)) == 0 ? SHA512_DIGESTINFO : 0;

	default:
		return 0;
	}
}

/****************************************************************************************************/

#define WHERE "PteidParsePrKDF"
DWORD PteidParsePrKDF(PCARD_DATA pCardData, DWORD *cbStream, BYTE *pbStream, WORD *cbKeySize) {
	DWORD dwReturn = 0;
	DWORD dwCounter = 0;
	DWORD dwInc = 0;
	*cbKeySize = 0;

	LogTrace(LOGTYPE_INFO, WHERE, "Enter API...");

	LogTrace(LOGTYPE_DEBUG, WHERE, "Contents of PrKDF:");
	LogDumpHex(*cbStream, pbStream);

	/********************/
	/* Check Parameters */
	/********************/
	if (pCardData == NULL) {
		LogTrace(LOGTYPE_ERROR, WHERE, "Invalid parameter [pCardData]");
		CLEANUP(SCARD_E_INVALID_PARAMETER);
	}
	if (pbStream == NULL) {
		LogTrace(LOGTYPE_ERROR, WHERE, "Invalid parameter [ppbStream]");
		CLEANUP(SCARD_E_INVALID_PARAMETER);
	}
	if (cbStream == NULL) {
		LogTrace(LOGTYPE_ERROR, WHERE, "Invalid parameter [cbStream]");
		CLEANUP(SCARD_E_INVALID_PARAMETER);
	}

	// Ghetto-style ASN-1 parser to obtain the keysize from PrK DF file
	if (pbStream[dwCounter] == 0x30) // 0x30 means sequence
	{
		LogTrace(LOGTYPE_DEBUG, WHERE, "sequence [0x30]");
		dwCounter++; // jump to sequence length
		LogTrace(LOGTYPE_DEBUG, WHERE, "sequence length [0x%.2X]", pbStream[dwCounter]);
		dwInc = pbStream[dwCounter];
		dwCounter += dwInc; // add length (to jump over sequence)
		if (dwCounter < (*cbStream)) {
			// the last 2 bytes are the key size
			*cbKeySize = (pbStream[dwCounter - 1]) * 256;
			*cbKeySize += (pbStream[dwCounter]);
			LogTrace(LOGTYPE_INFO, WHERE, "PrK DF parser: obtained RSA key size = %d bits", *cbKeySize);
		} else {
			LogTrace(LOGTYPE_ERROR, WHERE, "*cbStream = %d dwCounter = %d", *cbStream, dwCounter);
			LogDumpHex(*cbStream, pbStream);
			CLEANUP(0x00FEFE);
		}
	} else {
		LogTrace(LOGTYPE_ERROR, WHERE, "Expected 0x30 instead of ox%.2x", pbStream[dwCounter]);
		LogDumpHex(*cbStream, pbStream);
		CLEANUP(0x00FEFE);
	}

cleanup:
	LogTrace(LOGTYPE_INFO, WHERE, "Exit API...");
	return (dwReturn);
}
#undef WHERE

#define WHERE "PteidReadPrKDF"
DWORD PteidReadPrKDF(PCARD_DATA pCardData, DWORD *out_len, PBYTE *data) {
	DWORD dwReturn = 0;
	unsigned char recvbuf[1024];
	DWORD recvlen = sizeof(recvbuf);
	unsigned char Cmd[128];
	DWORD dwCounter = 0;
	unsigned int uiCmdLg = 0;
	BYTE SW1, SW2;
	SCARD_IO_REQUEST ioSendPci = *g_pioSendPci;

	/***************/
	/* Select File */
	/***************/
	Cmd[0] = 0x00;
	Cmd[1] = 0xA4; /* SELECT COMMAND */
	Cmd[2] = 0x00;
	Cmd[3] = 0x0C;
	Cmd[4] = 0x02;
	Cmd[5] = 0x3F;
	Cmd[6] = 0x00;
	uiCmdLg = 7;

	dwReturn = SCardTransmit(pCardData->hScard, &ioSendPci, Cmd, uiCmdLg, NULL, recvbuf, &recvlen);

	SW1 = recvbuf[recvlen - 2];
	SW2 = recvbuf[recvlen - 1];

	// Missing select applet
	if (SW1 == 0x6A && SW2 == 0x86) {
		dwReturn = PteidSelectApplet(pCardData);
		if (dwReturn != SCARD_S_SUCCESS) {
			CLEANUP(dwReturn);
		}
	}

	Cmd[5] = 0x5F;

	dwReturn = SCardTransmit(pCardData->hScard, &ioSendPci, Cmd, uiCmdLg, NULL, recvbuf, &recvlen);

	// Obtain the file FCI template
	Cmd[3] = 0x00;
	Cmd[5] = 0xEF;
	if (card_type == IAS_V5_CARD)
		Cmd[6] = 0x0E;
	else
		Cmd[6] = 0x0D;

	dwReturn = SCardTransmit(pCardData->hScard, &ioSendPci, Cmd, uiCmdLg, NULL, recvbuf, &recvlen);

	SW1 = recvbuf[recvlen - 2];
	SW2 = recvbuf[recvlen - 1];
	if (SW1 == 0x61) {

		Cmd[0] = 0x00;
		Cmd[1] = 0xC0; /* GET RESPONSE command */
		Cmd[2] = 0x00;
		Cmd[3] = 0x00;
		Cmd[4] = SW2;

		uiCmdLg = 5;
		// Make all the buffer available to the next SCardTransmit call
		recvlen = sizeof(recvbuf);

		dwReturn = SCardTransmit(pCardData->hScard, &ioSendPci, Cmd, uiCmdLg, NULL, recvbuf, &recvlen);
	}

	if (dwReturn != SCARD_S_SUCCESS) {
		LogTrace(LOGTYPE_ERROR, WHERE, "Error reading PrkDF file metadata. Error code: [0x%02X]", dwReturn);
		CLEANUP(dwReturn);
	}

	// Default value for the size of PrkDF
	*out_len = 141;
	if (recvlen > 2) {

		while (dwCounter < recvlen - 3) {
			// Parse the sequence 82 02 XX XX where XX XX is the file size in bytes
			if (recvbuf[dwCounter] == 0x81 && recvbuf[dwCounter + 1] == 0x02) {
				*out_len = recvbuf[dwCounter + 2] * 256 + recvbuf[dwCounter + 3];
				LogTrace(LOGTYPE_DEBUG, WHERE, "out_len parsed from FCI is %d", *out_len);
				break;
			}
			dwCounter++;
		}
	}

	// We need to parse the PrkD File to get the private key length which is also the signature length
	dwReturn = PteidReadFile(pCardData, 0, out_len, recvbuf);

	LogTrace(LOGTYPE_DEBUG, WHERE, "out_len returned is %d", *out_len);

	if (dwReturn != SCARD_S_SUCCESS) {
		LogTrace(LOGTYPE_ERROR, WHERE, "Error reading PrkDF file. Error code: [0x%02X]", dwReturn);
		CLEANUP(dwReturn);
	}
	*data = (PBYTE)pCardData->pfnCspAlloc(*out_len);

	memcpy(*data, recvbuf, *out_len);

cleanup:
	return (dwReturn);
}
#undef WHERE

/****************************************************************************************************/

#define WHERE "PteidReadFile"
DWORD PteidReadFile(PCARD_DATA pCardData, DWORD dwOffset, DWORD *cbStream, PBYTE pbStream) {
	DWORD dwReturn = 0;

	SCARD_IO_REQUEST ioSendPci = *g_pioSendPci;
	// SCARD_IO_REQUEST  ioRecvPci = {1, sizeof(SCARD_IO_REQUEST)};

	unsigned char Cmd[128];
	unsigned int uiCmdLg = 0;

	unsigned char recvbuf[256];
	unsigned long recvlen = sizeof(recvbuf);
	BYTE SW1, SW2;

	DWORD cbRead = 0;
	DWORD cbPartRead = 0;

	/***************/
	/* Read File */
	/***************/
	Cmd[0] = 0x00;
	Cmd[1] = 0xB0; /* READ BINARY COMMAND */
	Cmd[2] = 0x00;
	Cmd[3] = 0x00;
	Cmd[4] = 0x00;
	uiCmdLg = 5;

	while ((*cbStream - cbRead) > 0) {
		Cmd[2] = (BYTE)((dwOffset + cbRead) >> 8); /* set reading startpoint     */
		Cmd[3] = (BYTE)(dwOffset + cbRead);

		cbPartRead = *cbStream - cbRead;
		if (cbPartRead > PTEID_READ_BINARY_MAX_LEN) /*if more than maximum length */
		{
			Cmd[4] = PTEID_READ_BINARY_MAX_LEN; /* is requested, than read    */
		} else									/* maximum length             */
		{
			Cmd[4] = (BYTE)(cbPartRead);
		}
		recvlen = sizeof(recvbuf);
		dwReturn = SCardTransmit(pCardData->hScard, &ioSendPci, Cmd, uiCmdLg, NULL, recvbuf, &recvlen);
		SW1 = recvbuf[recvlen - 2];
		SW2 = recvbuf[recvlen - 1];
		// PteidDelayAndRecover(pCardData, SW1, SW2, dwReturn);
		if (dwReturn != SCARD_S_SUCCESS) {
			LogTrace(LOGTYPE_ERROR, WHERE, "SCardTransmit errorcode: [0x%02X]", dwReturn);
			CLEANUP(dwReturn);
		}

		if ((SW1 == 0x62) && (SW2 == 0x82)) {
			LogTrace(LOGTYPE_INFO, WHERE, "PteidReadFile: end of file reached!");
			break;
		}

		/* Special case: when SW1 == 0x6C (=incorrect value of Le), we will
		retransmit with SW2 as Le, if SW2 is smaller then the
		PTEID_READ_BINARY_MAX_LEN */
		if ((SW1 == 0x6c) && (SW2 <= PTEID_READ_BINARY_MAX_LEN)) {
			Cmd[4] = SW2;
			recvlen = sizeof(recvbuf);
			dwReturn = SCardTransmit(pCardData->hScard, &ioSendPci, Cmd, uiCmdLg, NULL, recvbuf, &recvlen);
			if (dwReturn != SCARD_S_SUCCESS) {
				LogTrace(LOGTYPE_ERROR, WHERE, "SCardTransmit errorcode: [0x%02X]", dwReturn);
				CLEANUP(dwReturn);
			}
			SW1 = recvbuf[recvlen - 2];
			SW2 = recvbuf[recvlen - 1];
		}

		if ((SW1 != 0x90) || (SW2 != 0x00)) {

			LogTrace(LOGTYPE_ERROR, WHERE, "Read Binary Failed: [0x%02X][0x%02X]", SW1, SW2);
			CLEANUP(dwReturn);
		}

		memcpy(pbStream + cbRead, recvbuf, recvlen - 2);
		cbRead += recvlen - 2;
	}
	*cbStream = cbRead;
cleanup:
	return (dwReturn);
}
#undef WHERE

/****************************************************************************************************/
#define WHERE "PteidSelectApplet"
/**/
DWORD PteidSelectApplet(PCARD_DATA pCardData) {
	DWORD dwReturn = 0;

	SCARD_IO_REQUEST ioSendPci = *g_pioSendPci;
	// SCARD_IO_REQUEST  ioRecvPci = {1, sizeof(SCARD_IO_REQUEST)};

	unsigned char Cmd[128];
	unsigned int uiCmdLg = 0;

	unsigned char recvbuf[256];
	unsigned long recvlen = sizeof(recvbuf);
	BYTE SW1, SW2;
	BYTE IAS_PTEID_APPLET_AID[] = {0x60, 0x46, 0x32, 0xFF, 0x00, 0x01, 0x02};
	BYTE IAS_V5_PTEID_APPLET_AID[] = {0x60, 0x46, 0x32, 0xFF, 0x00, 0x00, 0x04};
	BYTE GEMSAFE_APPLET_AID[] = {0x60, 0x46, 0x32, 0xFF, 0x00, 0x00, 0x02};
	BYTE cAppletID = sizeof(IAS_PTEID_APPLET_AID);

	int i = 0;

	/***************/
	/* Select File */
	/***************/
	Cmd[0] = 0x00;
	Cmd[1] = 0xA4; /* SELECT COMMAND 00 A4 04 0C 07 */
	Cmd[2] = 0x04;
	if (card_type == GEMSAFE_CARD || IAS_V5_CARD)
		Cmd[3] = 0x00;
	else
		Cmd[3] = 0x0C;
	Cmd[4] = 0x07;

	if (card_type == GEMSAFE_CARD)
		memcpy(&Cmd[5], GEMSAFE_APPLET_AID, sizeof(GEMSAFE_APPLET_AID));
	else if (card_type == IAS_V5_CARD)
		memcpy(&Cmd[5], IAS_V5_PTEID_APPLET_AID, sizeof(IAS_V5_PTEID_APPLET_AID));
	else
		memcpy(&Cmd[5], IAS_PTEID_APPLET_AID, cAppletID);

	uiCmdLg = 5 + cAppletID;

	dwReturn = SCardTransmit(pCardData->hScard, &ioSendPci, Cmd, uiCmdLg, NULL, recvbuf, &recvlen);
	SW1 = recvbuf[recvlen - 2];
	SW2 = recvbuf[recvlen - 1];
	PteidDelayAndRecover(pCardData, SW1, SW2, dwReturn);
	if (dwReturn != SCARD_S_SUCCESS) {
		LogTrace(LOGTYPE_ERROR, WHERE, "SCardTransmit errorcode: [0x%02X]", dwReturn);
		CLEANUP(dwReturn);
	}

	if ((SW1 != 0x90) || (SW2 != 0x00)) {
		LogTrace(LOGTYPE_ERROR, WHERE, "Select Applet Failed (wrong smartcard type?): [0x%02X][0x%02X]", SW1, SW2);

		if ((SW1 != 0x90) || (SW2 != 0x00)) {
			LogTrace(LOGTYPE_ERROR, WHERE, "Select Applet Failed (wrong smartcard type?): [0x%02X][0x%02X]", SW1, SW2);
			CLEANUP(dwReturn);
		}
	}

cleanup:
	return (dwReturn);
}
#undef WHERE

/****************************************************************************************************/

/* CCID Features */
#define WHERE "CCIDfindFeature"
DWORD CCIDfindFeature(BYTE featureTag, BYTE *features, DWORD featuresLength) {
	DWORD idx = 0;
	int count;
	while (idx < featuresLength) {
		BYTE tag = features[idx];
		idx++;
		idx++;
		if (featureTag == tag) {
			DWORD feature = 0;
			for (count = 0; count < 3; count++) {
				feature |= features[idx] & 0xff;
				idx++;
				feature <<= 8;
			}
			feature |= features[idx] & 0xff;
			return feature;
		}
		idx += 4;
	}
	return 0;
}
#undef WHERE

/****************************************************************************************************/

#define WHERE "CCIDgetFeatures"
DWORD CCIDgetFeatures(PFEATURES pFeatures, SCARDHANDLE hCard) {
	BYTE pbRecvBuffer[200];
	DWORD dwRecvLength, dwReturn;
	pFeatures->VERIFY_PIN_START = 0;
	pFeatures->VERIFY_PIN_FINISH = 0;
	pFeatures->VERIFY_PIN_DIRECT = 0;
	pFeatures->MODIFY_PIN_START = 0;
	pFeatures->MODIFY_PIN_FINISH = 0;
	pFeatures->MODIFY_PIN_DIRECT = 0;
	pFeatures->GET_KEY_PRESSED = 0;
	pFeatures->ABORT = 0;

	dwReturn = SCardControl(hCard, SCARD_CTL_CODE(3400), NULL, 0, pbRecvBuffer, sizeof(pbRecvBuffer), &dwRecvLength);
	if (SCARD_S_SUCCESS != dwReturn) {
		LogTrace(LOGTYPE_ERROR, WHERE, "CCIDgetFeatures errorcode: [0x%02X]", dwReturn);
		CLEANUP(dwReturn);
	}
	pFeatures->VERIFY_PIN_START = CCIDfindFeature(FEATURE_VERIFY_PIN_START, pbRecvBuffer, dwRecvLength);
	pFeatures->VERIFY_PIN_FINISH = CCIDfindFeature(FEATURE_VERIFY_PIN_FINISH, pbRecvBuffer, dwRecvLength);
	pFeatures->VERIFY_PIN_DIRECT = CCIDfindFeature(FEATURE_VERIFY_PIN_DIRECT, pbRecvBuffer, dwRecvLength);
	pFeatures->MODIFY_PIN_START = CCIDfindFeature(FEATURE_MODIFY_PIN_START, pbRecvBuffer, dwRecvLength);
	pFeatures->MODIFY_PIN_FINISH = CCIDfindFeature(FEATURE_MODIFY_PIN_FINISH, pbRecvBuffer, dwRecvLength);
	pFeatures->MODIFY_PIN_DIRECT = CCIDfindFeature(FEATURE_MODIFY_PIN_DIRECT, pbRecvBuffer, dwRecvLength);
	pFeatures->GET_KEY_PRESSED = CCIDfindFeature(FEATURE_GET_KEY_PRESSED, pbRecvBuffer, dwRecvLength);
	pFeatures->ABORT = CCIDfindFeature(FEATURE_ABORT, pbRecvBuffer, dwRecvLength);
cleanup:
	return (dwReturn);
}

#undef WHERE

#define WHERE "createVerifyCommandGemPC"
DWORD createVerifyCommandGemPC(PPIN_VERIFY_STRUCTURE pVerifyCommand, unsigned int pin_ref) {
	char padding = 0;

	LogTrace(LOGTYPE_INFO, WHERE, "createVerifyCommandGemPC(): pinRef = %d", pin_ref);
	pVerifyCommand->bTimeOut = 30;
	pVerifyCommand->bTimeOut2 = 30;
	pVerifyCommand->bmFormatString = 0x82;
	pVerifyCommand->bmPINBlockString = 0x00;
	pVerifyCommand->bmPINLengthFormat = 0x00;
	pVerifyCommand->wPINMaxExtraDigit = 0x0408; /* Min Max */

	pVerifyCommand->bEntryValidationCondition = 0x02;
	/* validation key pressed */
	pVerifyCommand->bNumberMessage = 0x01;

	pVerifyCommand->wLangId = 0x0816; // Code smell #2
	pVerifyCommand->bMsgIndex = 0x00;
	(pVerifyCommand->bTeoPrologue)[0] = 0x00;
	pVerifyCommand->bTeoPrologue[1] = 0x00;
	pVerifyCommand->bTeoPrologue[2] = 0x00;

	pVerifyCommand->abData[0] = 0x00;	 // CLA
	pVerifyCommand->abData[1] = 0x20;	 // INS Verify
	pVerifyCommand->abData[2] = 0x00;	 // P1
	pVerifyCommand->abData[3] = pin_ref; // P2
	pVerifyCommand->abData[4] = 0x08;	 // Lc = 8 bytes in command data
	padding = card_type != IAS_CARD ? 0xFF : 0x2F;
	pVerifyCommand->abData[5] = padding;
	pVerifyCommand->abData[6] = padding;  // Pin[1]
	pVerifyCommand->abData[7] = padding;  // Pin[2]
	pVerifyCommand->abData[8] = padding;  // Pin[3]
	pVerifyCommand->abData[9] = padding;  // Pin[4]
	pVerifyCommand->abData[10] = padding; // Pin[5]
	pVerifyCommand->abData[11] = padding; // Pin[6]
	pVerifyCommand->abData[12] = padding; // Pin[7]

	pVerifyCommand->ulDataLength = 13;

	return 0;
}

#undef WHERE

#define WHERE "createVerifyCommand"
DWORD createVerifyCommandACR83(PPIN_VERIFY_STRUCTURE pVerifyCommand, unsigned int pin_ref) {
	char padding = 0;

	LogTrace(LOGTYPE_INFO, WHERE, "createVerifyCommandACR83(): pinRef = %d", pin_ref);
	pVerifyCommand->bTimeOut = 0x00;
	pVerifyCommand->bTimeOut2 = 0x00;
	pVerifyCommand->bmFormatString = 0x82;
	pVerifyCommand->bmPINBlockString = 0x08;
	pVerifyCommand->bmPINLengthFormat = 0x00;
	pVerifyCommand->wPINMaxExtraDigit = 0x0408; /* Min Max */

	pVerifyCommand->bEntryValidationCondition = 0x02;
	/* validation key pressed */
	pVerifyCommand->bNumberMessage = 0x01;

	pVerifyCommand->wLangId = 0x0409; // Code smell #2
	pVerifyCommand->bMsgIndex = 0x00;
	(pVerifyCommand->bTeoPrologue)[0] = 0x00;
	pVerifyCommand->bTeoPrologue[1] = 0x00;
	pVerifyCommand->bTeoPrologue[2] = 0x00;

	pVerifyCommand->abData[0] = 0x00;	 // CLA
	pVerifyCommand->abData[1] = 0x20;	 // INS Verify
	pVerifyCommand->abData[2] = 0x00;	 // P1
	pVerifyCommand->abData[3] = pin_ref; // P2
	pVerifyCommand->abData[4] = 0x08;	 // Lc = 8 bytes in command data
	padding = card_type != IAS_CARD ? 0xFF : 0x2F;
	pVerifyCommand->abData[5] = padding;
	pVerifyCommand->abData[6] = padding;  // Pin[1]
	pVerifyCommand->abData[7] = padding;  // Pin[2]
	pVerifyCommand->abData[8] = padding;  // Pin[3]
	pVerifyCommand->abData[9] = padding;  // Pin[4]
	pVerifyCommand->abData[10] = padding; // Pin[5]
	pVerifyCommand->abData[11] = padding; // Pin[6]
	pVerifyCommand->abData[12] = padding; // Pin[7]

	pVerifyCommand->ulDataLength = 13;

	return 0;
}
#undef WHERE

/*  VERIFY command for generic Pinpad readers,
	it's known to work with the Gemalto IDBridge CT710 reader
*/

#define WHERE "createVerifyCommand"
DWORD createVerifyCommand(PPIN_VERIFY_STRUCTURE pVerifyCommand, unsigned int pin_ref) {
	char padding;
	LogTrace(LOGTYPE_INFO, WHERE, "createVerifyCommand(): pinRef = %d", pin_ref);
	pVerifyCommand->bTimeOut = 30;
	pVerifyCommand->bTimeOut2 = 30;
	pVerifyCommand->bmFormatString = 0x00 | 0x00 | 0x00 | 0x02;

	pVerifyCommand->bmPINBlockString = 0x08;
	pVerifyCommand->bmPINLengthFormat = 0x00;
	pVerifyCommand->wPINMaxExtraDigit = PTEID_MIN_USER_PIN_LEN << 8 | PTEID_MAX_USER_PIN_LEN;
	pVerifyCommand->bEntryValidationCondition = 0x02;
	pVerifyCommand->bNumberMessage = 0x01;
	pVerifyCommand->wLangId = 0x0409;
	/*
	 * We should support multiple languages for CCID devices with LCD screen
	 */
	pVerifyCommand->bMsgIndex = 0x00;
	/*
	 * 0x00 = PIN insertion prompt
	 */
	pVerifyCommand->bTeoPrologue[0] = 0x00;
	pVerifyCommand->bTeoPrologue[1] = 0x00;
	pVerifyCommand->bTeoPrologue[2] = 0x00;

	pVerifyCommand->abData[0] = 0x00;	 // CLA
	pVerifyCommand->abData[1] = 0x20;	 // INS Verify
	pVerifyCommand->abData[2] = 0x00;	 // P1
	pVerifyCommand->abData[3] = pin_ref; // P2
	pVerifyCommand->abData[4] = 0x08;	 // Lc = 8 bytes in command data
	padding = card_type != IAS_CARD ? 0xFF : 0x2F;

	pVerifyCommand->abData[5] = padding;  // Pin[1]
	pVerifyCommand->abData[6] = padding;  // Pin[2]
	pVerifyCommand->abData[7] = padding;  // Pin[3]
	pVerifyCommand->abData[8] = padding;  // Pin[4]
	pVerifyCommand->abData[9] = padding;  // Pin[5]
	pVerifyCommand->abData[10] = padding; // Pin[6]
	pVerifyCommand->abData[11] = padding; // Pin[7]
	pVerifyCommand->abData[12] = padding; // Pin[8]

	pVerifyCommand->ulDataLength = 13;

	return 0;
}
/****************************************************************************************************/
