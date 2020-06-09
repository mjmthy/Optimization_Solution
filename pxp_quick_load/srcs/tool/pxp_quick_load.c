#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/ioctl.h>

#define PXP_QL_GETINFO _IO('f', 0x70)
#define PXP_QL_REINIT  _IO('f', 0x71)
#define PXP_QL_RELEASE _IO('f', 0x72)
#define PXP_QL_SETSIZE _IO('f', 0x73)

struct pxp_ql_info {
        int size;
};

static char m_path[256];
static FILE *m;
int    pxp_ql;

static void help_msg()
{
	printf("pxp_quick_load usage:\n");
	printf("  -s store dara to CMA pool\n" 
	       "     (for local debug)\n");
	printf("  -r restore data from CMA pool\n");
	printf("  -f specify the path of data\n"
	       "     (use along with -s or -r)\n");
	printf("  -S specify the exact size of data to be restored\n"
	       "     (use along with -r)\n");
	printf("  -I reinitialize CMA pool\n");
	printf("  -R release CMA pool\n");
	printf("  -h show this help message\n");
}

int main(int argc, char *argv[])
{
	int opt = 0;
	int store = 0;
	int set_size = 0;
	int size;
	int release = 0;
	int reinit = 0;
	int m_size;
	void *m_addr;
	struct pxp_ql_info info;

	if (argc == 1 || argc == 3 || argc == 5 || argc > 6) {
		printf("wrong arguments\n");
		help_msg();
		exit(-1);
	}

	while ((opt = getopt(argc, argv, "srhS:RIf:")) != -1)
	{
		switch(opt)
		{
			case 's':
				store = 1;
				break;
			case 'r':
				store = 0;
				break;
			case 'S':
				set_size = 1;
				size = atoi(optarg);
				break;
			case 'R':
				release = 1;
				break;
			case 'I':
				reinit = 1;
				break;
			case 'f':
				strcpy(m_path, optarg);
				break;
			case 'h':
			default:
				help_msg();
				exit(-1);
		}
	}

	pxp_ql = open("/dev/pxp_ql", O_RDWR);
	if (pxp_ql <= 0) {
		printf("fail to open /dev/pxp_ql\n");
		exit(-1);
	}

	if (release) {
		ioctl(pxp_ql, PXP_QL_RELEASE, &info);
		close(pxp_ql);
		exit(0);
	}

	if (reinit) {
		ioctl(pxp_ql, PXP_QL_REINIT, &info);
		close(pxp_ql);	
		exit(0);
	}

	if (set_size) {
		info.size = size;
		if (ioctl(pxp_ql, PXP_QL_SETSIZE, &info)) {
			exit(-1);
		}
	}

	printf("%s %s\n", store ? "store": "restore", m_path);
	if (store) {
		m = fopen(m_path, "rb");
		if (m == NULL) {
			printf("fail to open file %s\n", m_path);
			exit(-1);
		}

		fseek(m, 0, SEEK_END);
		m_size = ftell(m);
		//printf("module size: %d Bytes\n", m_size);
		fseek(m, 0, SEEK_SET);

		m_addr = mmap(NULL, m_size, PROT_READ|PROT_WRITE, MAP_SHARED, pxp_ql, 0);
		if (m_addr == MAP_FAILED) {
			printf("fail to mmap\n");
			fclose(m);
			exit(-1);
		}
		fread(m_addr, sizeof(char), m_size, m);

		munmap(m_addr, m_size);
	} else {
		m = fopen(m_path, "wb");
		if (m == NULL) {
			printf("fail to open file %s\n", m_path);
			exit(-1);
		}
		ioctl(pxp_ql, PXP_QL_GETINFO, &info);
		//printf("module size: %d\n", info.size);
		m_size = info.size;
		if (m_size == 0) {
                        printf("no module saved before, for none module data restoration, "
                               "please set data size via -S\n");
			fclose(m);
			close(pxp_ql);
			exit(-1);
		}
		m_addr = mmap(NULL, m_size, PROT_READ|PROT_WRITE, MAP_SHARED, pxp_ql, 0);
		if (m_addr == MAP_FAILED) {
			printf("fail to mmap\n");
			fclose(m);
			close(pxp_ql);
			exit(-1);
		}
		fwrite(m_addr, sizeof(char), m_size, m);
		munmap(m_addr, m_size);
	}

	fclose(m);
	close(pxp_ql);

	return 0;
}
