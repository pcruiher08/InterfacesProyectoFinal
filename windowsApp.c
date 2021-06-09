#include <stdio.h>
#include <conio.h>
#include <stdlib.h>
#include <windows.h>
#include <SETUPAPI.H>

#define dim 8

//----------------------------------------------
#define RICH_VENDOR_ID			0x0000
#define RICH_USBHID_GENIO_ID	0x2019

#define INPUT_REPORT_SIZE	64
#define OUTPUT_REPORT_SIZE	64
//----------------------------------------------
typedef struct _HIDD_ATTRIBUTES {
	ULONG   Size; // = sizeof (struct _HIDD_ATTRIBUTES)
	USHORT  VendorID;
	USHORT  ProductID;
	USHORT  VersionNumber;
} HIDD_ATTRIBUTES, *PHIDD_ATTRIBUTES;

typedef VOID(__stdcall* PHidD_GetProductString)(HANDLE, PVOID, ULONG);
typedef VOID(__stdcall* PHidD_GetHidGuid)(LPGUID);
typedef BOOLEAN(__stdcall* PHidD_GetAttributes)(HANDLE, PHIDD_ATTRIBUTES);
typedef BOOLEAN(__stdcall* PHidD_SetFeature)(HANDLE, PVOID, ULONG);
typedef BOOLEAN(__stdcall* PHidD_GetFeature)(HANDLE, PVOID, ULONG);

//----------------------------------------------

HINSTANCE                       hHID = NULL;
PHidD_GetProductString          HidD_GetProductString = NULL;
PHidD_GetHidGuid                HidD_GetHidGuid = NULL;
PHidD_GetAttributes             HidD_GetAttributes = NULL;
PHidD_SetFeature                HidD_SetFeature = NULL;
PHidD_GetFeature                HidD_GetFeature = NULL;
HANDLE                          DeviceHandle = INVALID_HANDLE_VALUE;

unsigned int moreHIDDevices = TRUE;
unsigned int HIDDeviceFound = FALSE;
int vIDUsuario;
int pIDUsuario;
unsigned int terminaAbruptaEInstantaneamenteElPrograma = 0;

float matA[dim][dim];
float matB[dim][dim];
float matC[dim][dim];

void Load_HID_Library(void) {
	hHID = LoadLibrary("HID.DLL");
	if (!hHID) {
		printf("Failed to load HID.DLL\n");
		return;
	}

	HidD_GetProductString = (PHidD_GetProductString)GetProcAddress(hHID, "HidD_GetProductString");
	HidD_GetHidGuid = (PHidD_GetHidGuid)GetProcAddress(hHID, "HidD_GetHidGuid");
	HidD_GetAttributes = (PHidD_GetAttributes)GetProcAddress(hHID, "HidD_GetAttributes");
	HidD_SetFeature = (PHidD_SetFeature)GetProcAddress(hHID, "HidD_SetFeature");
	HidD_GetFeature = (PHidD_GetFeature)GetProcAddress(hHID, "HidD_GetFeature");

	if (!HidD_GetProductString
		|| !HidD_GetAttributes
		|| !HidD_GetHidGuid
		|| !HidD_SetFeature
		|| !HidD_GetFeature) {
		printf("Couldn't find one or more HID entry points\n");
		return;
	}
}

int Open_Device(void) {
	HDEVINFO							DeviceInfoSet;
	GUID								InterfaceClassGuid;
	SP_DEVICE_INTERFACE_DATA			DeviceInterfaceData;
	PSP_DEVICE_INTERFACE_DETAIL_DATA	pDeviceInterfaceDetailData;
	HIDD_ATTRIBUTES						Attributes;
	DWORD								Result;
	DWORD								MemberIndex = 0;
	DWORD								Required;

	//Validar si se "carg�" la biblioteca (DLL)
	if (!hHID)
		return (0);

	//Obtener el Globally Unique Identifier (GUID) para dispositivos HID
	HidD_GetHidGuid(&InterfaceClassGuid);
	//Sacarle a Windows la informaci�n sobre todos los dispositivos HID instalados y activos en el sistema
	// ... almacenar esta informaci�n en una estructura de datos de tipo HDEVINFO
	DeviceInfoSet = SetupDiGetClassDevs(&InterfaceClassGuid, NULL, NULL, DIGCF_PRESENT | DIGCF_INTERFACEDEVICE);
	if (DeviceInfoSet == INVALID_HANDLE_VALUE)
		return (0);

	//Obtener la interfaz de comunicaci�n con cada uno de los dispositivos para preguntarles informaci�n espec�fica
	DeviceInterfaceData.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);

	printf("A ver si es cierto %d %d", vIDUsuario, pIDUsuario);

	while (!HIDDeviceFound) {
		// ... utilizando la variable MemberIndex ir preguntando dispositivo por dispositivo ...
		moreHIDDevices = SetupDiEnumDeviceInterfaces(DeviceInfoSet, NULL, &InterfaceClassGuid,
			MemberIndex, &DeviceInterfaceData);
		if (!moreHIDDevices) {
			// ... si llegamos al fin de la lista y no encontramos al dispositivo ==> terminar y marcar error
			SetupDiDestroyDeviceInfoList(DeviceInfoSet);
			return (0); //No more devices found
		}
		else {
			//Necesitamos preguntar, a trav�s de la interfaz, el PATH del dispositivo, para eso ...
			// ... primero vamos a ver cu�ntos caracteres se requieren (Required)
			Result = SetupDiGetDeviceInterfaceDetail(DeviceInfoSet, &DeviceInterfaceData, NULL, 0, &Required, NULL);
			pDeviceInterfaceDetailData = (PSP_DEVICE_INTERFACE_DETAIL_DATA)malloc(Required);
			if (pDeviceInterfaceDetailData == NULL) {
				printf("Error en SetupDiGetDeviceInterfaceDetail\n");
				return (0);
			}
			//Ahora si, ya que el "buffer" fue preparado (pDeviceInterfaceDetailData{DevicePath}), vamos a preguntar PATH
			pDeviceInterfaceDetailData->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);
			Result = SetupDiGetDeviceInterfaceDetail(DeviceInfoSet, &DeviceInterfaceData, pDeviceInterfaceDetailData,
				Required, NULL, NULL);
			if (!Result) {
				printf("Error en SetupDiGetDeviceInterfaceDetail\n");
				free(pDeviceInterfaceDetailData);
				return(0);
			}
			//Para este momento ya sabemos el PATH del dispositivo, ahora hay que preguntarle ...
			// ... su VID y PID, para ver si es con quien nos interesa comunicarnos
			printf("Found? ==> ");
			printf("Device: %s\n", pDeviceInterfaceDetailData->DevicePath);

			//Obtener un "handle" al dispositivo
			DeviceHandle = CreateFile(pDeviceInterfaceDetailData->DevicePath,
				GENERIC_READ | GENERIC_WRITE,
				FILE_SHARE_READ | FILE_SHARE_WRITE,
				(LPSECURITY_ATTRIBUTES)NULL,
				OPEN_EXISTING,
				0,
				NULL);

			if (DeviceHandle == INVALID_HANDLE_VALUE) {
				printf("Error en el CreateFile!!!\n");
			}
			else {
				//Preguntar por los atributos del dispositivo
				Attributes.Size = sizeof(Attributes);
				Result = HidD_GetAttributes(DeviceHandle, &Attributes);
				if (!Result) {
					printf("Error en HIdD_GetAttributes\n");
					CloseHandle(DeviceHandle);
					free(pDeviceInterfaceDetailData);
					return(0);
				}
				//Analizar los atributos del dispositivo para verificar el VID y PID
				printf("MemberIndex=%d,VID=%04x,PID=%04x\n", MemberIndex, Attributes.VendorID, Attributes.ProductID);
				if ((Attributes.VendorID == RICH_VENDOR_ID) && (Attributes.ProductID == RICH_USBHID_GENIO_ID)) {
					printf("USB/HID GenIO ==> ");
					printf("Device: %s\n", pDeviceInterfaceDetailData->DevicePath);
					HIDDeviceFound = TRUE;
				}
				else
					CloseHandle(DeviceHandle);

			}
			MemberIndex++;
			free(pDeviceInterfaceDetailData);
			if (HIDDeviceFound) {
				printf("Dispositivo HID solicitado ... localizado!!!, presione <ENTER>\n");
				getchar();
			}
		}
	}
	return(1);
}

void readMatrix(float* matrix[8]) {

}

void printMatrix(float* matrix[8]) {
	for (int i = 0; i < 8; i++) {
		for (int j = 0; j < 8; j++) {
			printf("%f",matrix[i][j]);
		}
	}
}

void Close_Device(void) {
	if (DeviceHandle != NULL) {
		CloseHandle(DeviceHandle);
		DeviceHandle = NULL;
	}
}

char mensaje[256];
char mensajeProcesado[256];
char mensajeFinal[256];

int longitud; 
unsigned short llave;

int PIC32MZ() {
	DWORD BytesRead = 0;
	DWORD BytesWritten = 0;
	unsigned char reporteEntrada[INPUT_REPORT_SIZE + 1];
	unsigned char reporteSalida[OUTPUT_REPORT_SIZE + 1];
	int status = 0;
	float* intPtr;

	if (DeviceHandle == NULL)	//Validar que haya comunicacion con el dispositivo
		return 0;

	for (char i = 0; i < dim; i++) {
		reporteSalida[0] = 0x00;
		reporteSalida[1] = 0x60;
		reporteSalida[2] = i;
		for (int j = 0; j < dim; j++) {
			intPtr = (float*)&reporteSalida[3 + j * 4];
			*intPtr = matA[i][j];
		}

		status = WriteFile(DeviceHandle, reporteSalida, OUTPUT_REPORT_SIZE + 1, &BytesWritten, NULL);
		if (!status)
			printf("Error en el WriteFile %d %d\n", GetLastError(), BytesWritten);
	}

	for (char i = 0; i < dim; i++) {
		reporteSalida[0] = 0x00;
		reporteSalida[1] = 0x61;
		reporteSalida[2] = i;
		for (int j = 0; j < dim; j++) {
			intPtr = (float*)&reporteSalida[3 + j * 4];
			*intPtr = matB[i][j];
		}
		status = WriteFile(DeviceHandle, reporteSalida, OUTPUT_REPORT_SIZE + 1, &BytesWritten, NULL);
		if (!status)
			printf("Error en el WriteFile %d %d\n", GetLastError(), BytesWritten);
	}

	reporteSalida[0] = 0x00;
	reporteSalida[1] = 0x62;

	status = WriteFile(DeviceHandle, reporteSalida, OUTPUT_REPORT_SIZE + 1, &BytesWritten, NULL);
	if (!status)
		printf("Error en el WriteFile %d %d\n", GetLastError(), BytesWritten);

	for (char i = 0; i < dim; i++) {
		reporteSalida[0] = 0x00;
		reporteSalida[1] = 0x63;
		reporteSalida[2] = i;

		status = WriteFile(DeviceHandle, reporteSalida, OUTPUT_REPORT_SIZE + 1, &BytesWritten, NULL);
		if (!status)
			printf("Error en el WriteFile %d %d\n", GetLastError(), BytesWritten);

		memset(&reporteEntrada, 0, INPUT_REPORT_SIZE + 1);

		status = ReadFile(DeviceHandle, reporteEntrada, INPUT_REPORT_SIZE + 1, &BytesRead, NULL);
		if (!status)
			printf("Error en el ReadFile: %d\n", GetLastError());



		for (int j = 0; j < dim; j++) {
			//matC[i][j] = (reporteEntrada[3 + j*4] << 24) | (reporteEntrada[4 + j*4] << 16) | (reporteEntrada[5 + j*4] << 8) | (reporteEntrada[6 + j*4]);   //Si llega el dummy al host :0

			intPtr = (float*)&reporteEntrada[3 + j * 4];
			matC[i][j] = *intPtr;

		}
	}
	
	printf("El mensaje original es "); 
	puts(mensaje);


	for (int i = 0; i < 256; i+=2) {
		//enviamos

		reporteSalida[0] = 0x00;
		reporteSalida[1] = 0x70;

		*(short*)&reporteSalida[2] = llave;


		reporteSalida[4] = mensaje[i];
		reporteSalida[5] = mensaje[i + 1];

		status = WriteFile(DeviceHandle, reporteSalida, OUTPUT_REPORT_SIZE + 1, &BytesWritten, NULL);
		if (!status)
			printf("Error en el WriteFile %d %d\n", GetLastError(), BytesWritten);



		//recibimos
		status = ReadFile(DeviceHandle, reporteEntrada, INPUT_REPORT_SIZE + 1, &BytesRead, NULL);
		if (!status)
			printf("Error en el ReadFile: %d\n", GetLastError());


		mensajeProcesado[i] = reporteEntrada[4];
		mensajeProcesado[i + 1] = reporteEntrada[5];

		//imprimimos
		//printf("%c%c", reporteEntrada[4], reporteEntrada[5]);
	}

	for (int i = 0; i < longitud; i += 2) {
		//enviamos

		reporteSalida[0] = 0x00;
		reporteSalida[1] = 0x71;

		*(short*)&reporteSalida[2] = llave;


		reporteSalida[4] = mensajeProcesado[i];
		reporteSalida[5] = mensajeProcesado[i + 1];

		status = WriteFile(DeviceHandle, reporteSalida, OUTPUT_REPORT_SIZE + 1, &BytesWritten, NULL);
		if (!status)
			printf("Error en el WriteFile %d %d\n", GetLastError(), BytesWritten);



		//recibimos
		status = ReadFile(DeviceHandle, reporteEntrada, INPUT_REPORT_SIZE + 1, &BytesRead, NULL);
		if (!status)
			printf("Error en el ReadFile: %d\n", GetLastError());


		mensajeFinal[i] = reporteEntrada[4];
		mensajeFinal[i + 1] = reporteEntrada[5];
		//imprimimos
		//printf("%c%c", reporteEntrada[4], reporteEntrada[5]);
	}



	/*

	//mensaje deberia verse asi
	printf("\n asi deberia verse el mensaje \n");
	for (int i = 0; i < 256; i+=2) {
		//enviamos

		reporteSalida[0] = 0x00;
		reporteSalida[1] = 0x70;

		*(short*)&reporteSalida[2] = llave;


		reporteSalida[4] = mensaje[i];
		reporteSalida[5] = mensaje[i + 1];
		printf("%c%c", reporteSalida[4] ^ reporteSalida[2], reporteSalida[5] ^ reporteSalida[3]);



	}

	printf("\n");

	printf("\n asi deberia verse el mensaje \n");
	for (int i = 0; i < 256; i+=2) {
		//enviamos

		reporteSalida[0] = 0x00;
		reporteSalida[1] = 0x70;

		*(short*)&reporteSalida[2] = llave;

		reporteSalida[4] = mensaje[i];
		reporteSalida[5] = mensaje[i+1];

		printf("%c%c", reporteSalida[4] ^ reporteSalida[2] ^ reporteSalida[2], reporteSalida[5] ^ reporteSalida[3] ^ reporteSalida[3]);



	}

	printf("\n");
	*/

	return(1);

}

void main() {

	int flag = 0;
	FILE *a, *b;
	fopen_s(&a, "matrizA.txt", "r");
	fopen_s(&b, "matrizB.txt", "r"); 


	for (int i = 0; i < 8; i++) {
		for (int j = 0; j < 8; j++) {
			//scanf_s("%f", &matA[i][j]);
			fscanf_s(a, "%f", &matA[i][j]);
		}
	}
	for (int i = 0; i < 8; i++) {
		for (int j = 0; j < 8; j++) {
			//scanf_s("%f", &matB[i][j]);
			fscanf_s(b, "%f", &matB[i][j]);

		}
	}
	for (int i = 0; i < dim; i++) {
		for (int j = 0; j < dim; j++) {
			//matA[i][j] = 1.0;
			//matB[i][j] = 1.0;

			matC[i][j] = 0.0;
		}
	}
	fclose(a);
	fclose(b);
	printf("Dame un mensaje\n");
	fgets(mensaje, sizeof(mensaje), stdin);
	char trash;
	//scanf_s("%c", &trash);
	puts(mensaje);
	for (int i = 0; i < 256; i++) {
		if (mensaje[i] == '\0') {
			break;
		}
		longitud = i;
	}

	printf("Dame una llave (un numero entre 0 y 65535)\n");
	
	scanf_s("%hu",&llave);
	/*/---------------------------------------------------
		for (int i = 0; i < dim; i++) {
			for (int j = 0; j < dim; j++) {
				for (int k = 0; k < dim; k++) {
					matC[i][j] += matA[i][k] * matB[k][j];
				}
			}
		}
	---------------------------------------------------/*/

	Load_HID_Library();
	if (Open_Device()) {
		printf("Vamos bien\n");
		flag = PIC32MZ();
	}
	else {
		printf(">:(\n");
	}
	Close_Device();

	if (flag == 1) {
		for (int i = 0; i < dim; i++) {
			for (int j = 0; j < dim; j++) {
				printf("%f\t", matC[i][j]);
			}
			printf("\n");
		}
		printf("El mensaje procesado se ve asi: \n");
		for (int i = 0; i < longitud; i++) {
			printf("%c", mensajeProcesado[i]);
		}
		printf("\nEl mensaje final se ve asi: \n");

		for (int i = 0; i < longitud; i++) {
			printf("%c", mensajeFinal[i]);
		}
		printf("\n");
	}
	else {
		printf("Error UnU");
	}

}