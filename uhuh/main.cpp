#include <stdio.h>
#include <Windows.h>
#include <TlHelp32.h>
#include <chrono>
#include <thread>

/*
	Aici ne tinem si noi variabilele aranjate mai frumos
	
	deobicei cand se foloseste namespace se pune si un identificator, gen:
	namespace Variabile { } -> si ca sa accesez variabilele trebuia sa pun mereu Variabile::m_client
	doar daca nu foloseam using namespace Variabile

	also, in loc de namespace poti folosi structuri, e acelasi cacat, si structuri folosesc cu polyloader
	pentru ca si deoarece am [swap_blocks][/swap_blocks]
*/
namespace {
	unsigned long m_processid = NULL; // Fiecare proces are un PID (PROCESSID) - Task Manager -> Details -> PID
	unsigned long m_client = NULL; // Fisierul DLL pt client_panorama.dll
	unsigned long m_engine = NULL; // Fisierul DLL pt engine.dll
	void* m_process = NULL; // Il folosim pentru OpenProcess, ca sa putem scrie si sa citim din proces

	unsigned long dwLocalPlayer = 0xD36B94; // localplayer = noi
	unsigned long dwForceJump = 0x51F4DB0; // pentru bhop
}

/*
	Poate o sa ti se para ciudat ca folosesc decltype(auto), adica ce e aceasta incantatie?
	well, i'm an edgy boy si in loc de unsigned long Modul folosest decltype(auto) care face acelasi cacat
	doar ca nu mai trebuie sa scriu eu unsigned long

	Modul(unsigned long pID, const char* pNume)
		pID -> pentru process id-ul de care am vorbit mai sus
		pNume -> pentru numele modulului pe care-l cautam
*/
decltype(auto) Modul(unsigned long pID, const char* pNume) {
	// Asta e un must la fiecare external
	void* m_help = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, pID);
	MODULEENTRY32 m32;
	m32.dwSize = sizeof(MODULEENTRY32);

	/*
		Aici vom lucra cu siruri si vom compara
		m32.szModule -> pentru ca am dat la m_help processid-ul de la csgo, el va cauta toate modulele asociate cu csgo
		daca vrei sa le vezi poti folosi acest cod (il pui dupa do)
			printf("modul [ %d ]\n", (const char*)m32.szModule); *
		
		strcmp(m32.szMdoule, pNume) -> comparam toate modulele disponibile cu ceea ce cautam noi
		dupa ce am gasit, atunci inchidem an open object handle, which is m_help
		dupa aceea vom da return la adresa unde am gasit modulul cautat initial de catre noi
	*/
	do {
		printf("Module [%s]\n", (const char*)m32.szModule);
		if (!strcmp((const char*)m32.szModule, pNume)) {
			printf("Modul [ %s ] a fost gasit la adresa [ %d ]\n", (const char*)m32.szModule, (unsigned long)m32.modBaseAddr);
			CloseHandle(m_help);
			return (unsigned long)m32.modBaseAddr;
		}
	} while (Module32Next(m_help, &m32));
}

decltype(auto) Proces(unsigned long pID, const char* pNume) {
	void* m_createtool = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, pID);

	PROCESSENTRY32 p32;
	p32.dwSize = sizeof(PROCESSENTRY32);

	/*
		Intr-un fel e acelasi cacat, doar ca acum nu mai lucram cu modulurile din proces, ci cautam procesul propriu zis in lista de procese
		Poti sa vezi toate procesele active in Task Manager -> Details

		Daca vrei sa vezi cum face cautarea atunci dupa while (...) { } adauga codul asta:
			printf("proces [ %d ]\n", p32.szExeFile);

		p32.th32ProcessID este PID-ul pe care-l cautam, cel pe care il are csgo.exe (se schimba la fiecare rulare)
		m_process = OpenProcess(...) -> cu vom avea acces sa scriem si sa citim
		m_processid = p32.th32ProcessID -> stocam pid-ul pentru Modul(pid, nume)
	*/
	while (Process32Next(m_createtool, &p32)) {
		printf("Searching through process: [%s]\n", p32.szExeFile);
		if (!strcmp((const char*)p32.szExeFile, pNume)) {
			m_process = OpenProcess(PROCESS_ALL_ACCESS, FALSE, p32.th32ProcessID);
			m_processid = p32.th32ProcessID;

			CloseHandle(m_createtool);
			return true;
		}
	}

	CloseHandle(m_createtool);
	return false;
}

namespace {
	/*
		Folosim template <class Ptr> pentru ca e mai usor, exemplu:
		
		Fara template <class Ptr> si functia de mai jos:
			ReadProcessMemory(m_process, (LPVOID)(m_client + dwLocalPlayer), &4, sizeof(int), 0);
		
		Cu template:
			Citeste<int>((m_client + dwLocalPlayer);

		Practic, imi returneaza buffer-ul fara sa il mai declar eu ( cum a fost cu 4 si sizeof(int)) since 4 e integer
	*/
	template <class Ptr>
	Ptr Citeste(unsigned long pAdresa) {
		Ptr m_ptr;
		ReadProcessMemory(m_process, (LPCVOID)pAdresa, &m_ptr, sizeof(m_ptr), 0);
		return m_ptr;
	}

	template <class Ptr>
	decltype(auto) Scrie(unsigned long pAdresa, Ptr m_ptr) {
		WriteProcessMemory(m_process, (LPVOID)pAdresa, &m_ptr, sizeof(m_ptr), 0);
	}
}

decltype(auto) Bhop() {
	/*
		std::this_thread::sleep_for(std::chrono::milliseconds(i)) e Sleep(i) doar ca mai edgy :))
		Folosim sleep pentru un memory usage si processor usage mai mic, cu atat mai mic cu atat mai bine
		Practic, folosim Sleep pentru optimizare

		Iara, de ce folosesc auto la variabile? Pentru ca imi e lene sa le definesc cu int, bool, etc
		Asa ca cu auto, dupa ce scriu cu ce este egal, auto va face deductia si imi va da acelasi typename pe care l-as introduce eu manual
	*/
	std::this_thread::sleep_for(std::chrono::milliseconds(1));
	auto m_flag = Citeste<int>(Citeste<unsigned long>(m_client + dwLocalPlayer) + 0x104);
	if (m_flag & (1 << 0) && GetAsyncKeyState(VK_SPACE)) {
		Scrie<unsigned long>(m_client + dwForceJump, 6);
	}
}

int main() {
	/*
		Daca gasim csgo.exe in lista de procese, atunci fa ce e mai jos
		Altfel iesi din program
	*/
	if (Proces(m_processid, "csgo.exe")) {
		// Initializam m_client pentru cand scriem si citim
		m_client = Modul(m_processid, "client.dll");
		// Daca m_client nu ia nicio valoare, aka daca nu gasim client_panorama atunci iesim din program
		if (!m_client)
			return 1;
		
		// Initializam m_engine pentru cand scriem si citim
		m_engine = Modul(m_processid, "engine.dll");
		// Daca m_engine nu ia nicio valoare, aka daca nu gasim engine atunci iesim din program
		if (!m_engine)
			return 1;

		// Cat timp nu apasam tasta DELETE atunci
		while (!GetAsyncKeyState(VK_DELETE)) {
			// folosesc static bool ca sa nu mai declar in namespace, plus ca folosesc doar in loop-ul asta
			static bool m_bBhop = false;
			// Daca apas tasta F8 atunci
			if (GetAsyncKeyState(VK_F8)) {
				/*
					Edgy wohooo:)))
					m_bBhop e initializat cu false in prima faza
					Dupa ce am apasat f8 o sa se transcrie ceva de genu:

					false = true;

					Practic, daca apas F8 se va schimba din true in false si din false in true
					Ca sa putem opri diferite features and shit
				*/
				m_bBhop = !m_bBhop;
			}
			
			// Afisam valoarea m_bBhop ca sa stim cand este activat si cand nu
			printf("m_bBhop [ %d ]\n", m_bBhop);
			
			// Daca m_bBhop == true
			if (m_bBhop) {
				// Walla merge bhop-ul :)
				Bhop();
			}

		//	std::this_thread::sleep_for(std::chrono::milliseconds(100));
		}
	} else {
		// Daca nu am gasit csgo.exe in lista de procese, atunci iesim din program
		printf("csgo.exe nu a fost gasit.\n");
		return 1;
	}

	return 0;
}
