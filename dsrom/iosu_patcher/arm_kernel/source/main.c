#include "types.h"
#include "utils.h"
#include "reload.h"
#include "elf_patcher.h"
#include "../../payload/arm_user_bin.h"
#include "getbins.h"

static const char repairData_set_fault_behavior[] = {
	0xE1,0x2F,0xFF,0x1E,0xE9,0x2D,0x40,0x30,0xE5,0x93,0x20,0x00,0xE1,0xA0,0x40,0x00,
	0xE5,0x92,0x30,0x54,0xE1,0xA0,0x50,0x01,0xE3,0x53,0x00,0x01,0x0A,0x00,0x00,0x02,
	0xE1,0x53,0x00,0x00,0xE3,0xE0,0x00,0x00,0x18,0xBD,0x80,0x30,0xE3,0x54,0x00,0x0D,
};
static const char repairData_set_panic_behavior[] = {
	0x08,0x16,0x6C,0x00,0x00,0x00,0x18,0x0C,0x08,0x14,0x40,0x00,0x00,0x00,0x9D,0x70,
	0x08,0x16,0x84,0x0C,0x00,0x00,0xB4,0x0C,0x00,0x00,0x01,0x01,0x08,0x14,0x40,0x00,
	0x08,0x15,0x00,0x00,0x08,0x17,0x21,0x80,0x08,0x17,0x38,0x00,0x08,0x14,0x30,0xD4,
	0x08,0x14,0x12,0x50,0x08,0x14,0x12,0x94,0xE3,0xA0,0x35,0x36,0xE5,0x93,0x21,0x94,
	0xE3,0xC2,0x2E,0x21,0xE5,0x83,0x21,0x94,0xE5,0x93,0x11,0x94,0xE1,0x2F,0xFF,0x1E,
	0xE5,0x9F,0x30,0x1C,0xE5,0x9F,0xC0,0x1C,0xE5,0x93,0x20,0x00,0xE1,0xA0,0x10,0x00,
	0xE5,0x92,0x30,0x54,0xE5,0x9C,0x00,0x00,
};
static const char repairData_usb_root_thread[] = {
	0xE5,0x8D,0xE0,0x04,0xE5,0x8D,0xC0,0x08,0xE5,0x8D,0x40,0x0C,0xE5,0x8D,0x60,0x10,
	0xEB,0x00,0xB2,0xFD,0xEA,0xFF,0xFF,0xC9,0x10,0x14,0x03,0xF8,0x10,0x62,0x4D,0xD3,
	0x10,0x14,0x50,0x00,0x10,0x14,0x50,0x20,0x10,0x14,0x00,0x00,0x10,0x14,0x00,0x90,
	0x10,0x14,0x00,0x70,0x10,0x14,0x00,0x98,0x10,0x14,0x00,0x84,0x10,0x14,0x03,0xE8,
	0x10,0x14,0x00,0x3C,0x00,0x00,0x01,0x73,0x00,0x00,0x01,0x76,0xE9,0x2D,0x4F,0xF0,
	0xE2,0x4D,0xDE,0x17,0xEB,0x00,0xB9,0x92,0xE3,0xA0,0x10,0x00,0xE3,0xA0,0x20,0x03,
	0xE5,0x9F,0x0E,0x68,0xEB,0x00,0xB3,0x20,
};

/* from smealum's iosuhax: must be placed at 0x05059938 */
static const char os_launch_hook[] = {
	0x47, 0x78, 0x00, 0x00, 0xe9, 0x2d, 0x40, 0x0f, 0xe2, 0x4d, 0xd0, 0x08, 0xeb,
	0xff, 0xfd, 0xfd, 0xe3, 0xa0, 0x00, 0x00, 0xeb, 0xff, 0xfe, 0x03, 0xe5, 0x9f,
	0x10, 0x4c, 0xe5, 0x9f, 0x20, 0x4c, 0xe3, 0xa0, 0x30, 0x00, 0xe5, 0x8d, 0x30,
	0x00, 0xe5, 0x8d, 0x30, 0x04, 0xeb, 0xff, 0xfe, 0xf1, 0xe2, 0x8d, 0xd0, 0x08,
	0xe8, 0xbd, 0x80, 0x0f, 0x2f, 0x64, 0x65, 0x76, 0x2f, 0x73, 0x64, 0x63, 0x61,
	0x72, 0x64, 0x30, 0x31, 0x00, 0x2f, 0x76, 0x6f, 0x6c, 0x2f, 0x73, 0x64, 0x63,
	0x61, 0x72, 0x64, 0x00, 0x00, 0x00, 0x2f, 0x76, 0x6f, 0x6c, 0x2f, 0x73, 0x64,
	0x63, 0x61, 0x72, 0x64, 0x00, 0x05, 0x11, 0x60, 0x00, 0x05, 0x0b, 0xe0, 0x00,
	0x05, 0x0b, 0xcf, 0xfc, 0x05, 0x05, 0x99, 0x70, 0x05, 0x05, 0x99, 0x7e,
};

extern const int from_cbhc;

#define LAUNCH_SYSMENU 0
#define LAUNCH_HBL 1
#define LAUNCH_MOCHA 2
#define LAUNCH_CFW_IMG 3

int _main()
{
	void(*invalidate_icache)() = (void(*)())0x0812DCF0;
	void(*invalidate_dcache)(unsigned int, unsigned int) = (void(*)())0x08120164;
	void(*flush_dcache)(unsigned int, unsigned int) = (void(*)())0x08120160;

	flush_dcache(0x081200F0, 0x4001); // giving a size >= 0x4000 flushes all cache

	int level = disable_interrupts();

	unsigned int control_register = disable_mmu();

	/* copy in ds vc title id to protect from installing/moving/deleting */
	kernel_memcpy((void*)(get_titleprot2_bin()+get_titleprot2_bin_len()-12), (void*)0x01E70108, 4);
	kernel_memcpy((void*)(get_titleprot_bin()+get_titleprot_bin_len()-8), (void*)0x01E70108, 4);

	/* save if we are booted from CBHC */
	kernel_memcpy((void*)(&from_cbhc), (void*)0x01E70110, 4);

	/* get value CBHC or Haxchi used to boot up */
	unsigned int launchmode = *(volatile int*)0x01E7010C;

	/* Save the request handle so we can reply later */
	*(volatile u32*)0x01E10000 = *(volatile u32*)0x1016AD18;

	/* Patch kernel_error_handler to BX LR immediately */
	*(volatile u32*)kernel_phys(0x08129A24) = 0xE12FFF1E;

	void * pset_fault_behavior = (void*)0x081298BC;
	kernel_memcpy(pset_fault_behavior, (void*)repairData_set_fault_behavior, sizeof(repairData_set_fault_behavior));

	void * pset_panic_behavior = (void*)0x081296E4;
	kernel_memcpy(pset_panic_behavior, (void*)repairData_set_panic_behavior, sizeof(repairData_set_panic_behavior));

	void * pusb_root_thread = (void*)0x10100174;
	kernel_memcpy(pusb_root_thread, (void*)repairData_usb_root_thread, sizeof(repairData_usb_root_thread));

	void * pUserBinSource = (void*)0x01E50000;
	void * pUserBinDest = (void*)0x101312D0;
	kernel_memcpy(pUserBinDest, (void*)pUserBinSource, sizeof(arm_user_bin));

	if(launchmode != LAUNCH_MOCHA)
	{
		// nop out memcmp hash checks
		*(volatile u32*)crypto_phys(0x040017E0) = 0xE3A00000; // mov r0, #0
		*(volatile u32*)crypto_phys(0x040019C4) = 0xE3A00000; // mov r0, #0
		*(volatile u32*)crypto_phys(0x04001BB0) = 0xE3A00000; // mov r0, #0
		*(volatile u32*)crypto_phys(0x04001D40) = 0xE3A00000; // mov r0, #0

		// patch OS launch sig check
		*(volatile u32*)mcp_phys(0x0500A818) = 0x20002000; // mov r0, #0; mov r0, #0

		// fix 10 minute timeout that crashes MCP after 10 minutes of booting
		*(volatile u32*)mcp_phys(0x05022474) = 0xFFFFFFFF; // NEW_TIMEOUT
	}

	if(launchmode != LAUNCH_MOCHA && launchmode != LAUNCH_CFW_IMG)
	{
		// jump to titleprot2_addr
		*(volatile u32*)mcp_phys(0x05014670) = 0xF0F9F99C; //bl titleprot2_addr

		// patch MCP authentication check
		*(volatile u32*)mcp_phys(0x05014CAC) = 0x20004770; // mov r0, #0; bx lr

		// replace ioctl 0x62 code with jump to wupserver
		*(volatile u32*)mcp_phys(0x05026BA8) = 0x47780000; // bx pc
		*(volatile u32*)mcp_phys(0x05026BAC) = 0xE59F1000; // ldr r1, [pc]
		*(volatile u32*)mcp_phys(0x05026BB0) = 0xE12FFF11; // bx r1
		*(volatile u32*)mcp_phys(0x05026BB4) = wupserver_addr; // wupserver code

		// patch system version number
		*(volatile u16*)mcp_phys(0x0502F29A) = 0x2363; // movs r3, #99
		*(volatile u16*)mcp_phys(0x0502F2AA) = 0x2363; // movs r3, #99
		*(volatile u16*)mcp_phys(0x0502F2BA) = 0x2363; // movs r3, #99

		// patch cert verification
		*(volatile u32*)mcp_phys(0x05052A90) = 0xE3A00000; // mov r0, #0
		*(volatile u32*)mcp_phys(0x05052A94) = 0xE12FFF1E; // bx lr

		// patch IOSC_VerifyPubkeySign to always succeed
		*(volatile u32*)mcp_phys(0x05052C44) = 0xE3A00000; // mov r0, #0
		*(volatile u32*)mcp_phys(0x05052C48) = 0xE12FFF1E; // bx lr

		// patch cached cert check
		*(volatile u32*)mcp_phys(0x05054D6C) = 0xE3A00000; // mov r0, 0
		*(volatile u32*)mcp_phys(0x05054D70) = 0xE12FFF1E; // bx lr

		// redirect mcp_debug_print to mcp_syslog_print (0x0503DCF0)
		*(volatile u32*)mcp_phys(0x05055454) = 0xEBFFA225; // bl 0x0503DCF0

		if(from_cbhc) // coldboot specific patches
		{
			// change system.xml to syshax.xml
			*(volatile u32*)mcp_rodata_phys(0x050600F0) = 0x79736861; // ysha
			*(volatile u32*)mcp_rodata_phys(0x050600F4) = 0x782E786D; // x.xm

			*(volatile u32*)mcp_rodata_phys(0x05060114) = 0x79736861; // ysha
			*(volatile u32*)mcp_rodata_phys(0x05060118) = 0x782E786D; // x.xm
		}

		// jump to titleprot_addr
		*(volatile u32*)mcp_d_r_phys(0x05107F70) = 0xF005FD0A; //bl titleprot_addr

		//free some mcp_d_r room for our code
		*(volatile u32*)mcp_d_r_phys(titleprot_addr-4) = 0x20004770; // mov r0, #0; bx lr
		// overwrite mcp_d_r code with titleprot
		kernel_memcpy((void*)mcp_d_r_phys(titleprot_addr), get_titleprot_bin(), get_titleprot_bin_len());
		invalidate_dcache(mcp_d_r_phys(titleprot_addr), get_titleprot_bin_len());

		// overwrite mcp_d_r code with titleprot2
		kernel_memcpy((void*)mcp_d_r_phys(titleprot2_addr), get_titleprot2_bin(), get_titleprot2_bin_len());
		invalidate_dcache(mcp_d_r_phys(titleprot2_addr), get_titleprot2_bin_len());
		invalidate_icache();

		//free some mcp_d_r room for our code
		*(volatile u32*)mcp_d_r_phys(wupserver_addr-4) = 0x47700000; //bx lr
		// overwrite mcp_d_r code with wupserver
		kernel_memcpy((void*)mcp_d_r_phys(wupserver_addr), get_wupserver_bin(), get_wupserver_bin_len());
		invalidate_dcache(mcp_d_r_phys(wupserver_addr), get_wupserver_bin_len());
		invalidate_icache();

		// apply IOS ELF launch hook (thanks dimok!)
		*(volatile u32*)kernel_phys(0x0812A120) = ARM_BL(0x0812A120, kernel_launch_ios);

		// allow any region title launch
		*(volatile u32*)acp_phys(0xE0030498) = 0xE3A00000; // mov r0, #0

		// allow custom bootLogoTex and bootMovie.h264
		*(volatile u32*)acp_phys(0xE0030D68) = 0xE3A00000; // mov r0, #0
		*(volatile u32*)acp_phys(0xE0030D34) = 0xE3A00000; // mov r0, #0
	}

	//custom fw.img reboot
	if(launchmode == LAUNCH_CFW_IMG)
	{
		//copy in new fw.img path
		int i;
		for (i = 0; i < 32; i++)
			if (i < 31)
				((char*)mcp_rodata_phys(0x050663B4))[i] = ((char*)0x01E70000)[i];
			else
				((char*)mcp_rodata_phys(0x050663B4))[i] = (char)0;

		// jump to launch_os_hook
		*(volatile u32*)mcp_phys(0x050282AE) = 0xF031FB43; // bl launch_os_hook

		// copy launch_os_hook into free mcp code space
		for (i = 0; i < sizeof(os_launch_hook); i++)
			((char*)mcp_phys(0x05059938))[i] = os_launch_hook[i];
	}

	if(from_cbhc) // coldboot specific patches
	{
		// patch default title id to system menu
		*(volatile u32*)mcp_data_phys(0x050B817C) = *(volatile u32*)0x01E70100;
		*(volatile u32*)mcp_data_phys(0x050B8180) = *(volatile u32*)0x01E70104;

		// force check USB storage on load
		*(volatile u32*)acp_phys(0xE012202C) = 0x00000001; // find USB flag
	}

	*(volatile u32*)(0x1555500) = 0;

	/* REENABLE MMU */
	restore_mmu(control_register);

	invalidate_dcache(0x081298BC, 0x4001); // giving a size >= 0x4000 invalidates all cache
	invalidate_icache();

	enable_interrupts(level);

	return 0;
}
