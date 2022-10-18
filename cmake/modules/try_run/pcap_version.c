#include <pcap/pcap.h>
#include <stdio.h>

int main(int argc, char* argv[]) {
	const char* ver_str = pcap_lib_version();
	if (!ver_str) {
		return 1;
	}

	printf("%s", ver_str);
	return 0;
}
