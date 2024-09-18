/*
 * main_udoserver.cpp
 *
 *  Created on: Sep 18, 2024
 *      Author: vnagy
 */

#include "stdio.h"
#include "prgconfig.h"
#include "udo_comm.h"
#include "commh_udosl.h"
#include "udo_ip_comm.h"
#include "wait_for_udo.h"

int main(int argc, char * const * argv)
{
	printf("UDOSERVER...\r\n");

  if (argc < 1)
  {
  	printf("config file argument is missing.\n");
  	return 1;
  }

  printf("Config file: \"%s\"\n", argv[1]);

  if (!prgconfig.ReadConfigFile(argv[1]))
  {
  	return 1;
  }

  printf("Serial port: \"%s\"\n", prgconfig.udosl_devaddr.c_str());

  udosl_commh.devstr = prgconfig.udosl_devaddr;
  udocomm.SetHandler(&udosl_commh);

  printf("Connecting to device ...\n");

  udocomm.Open();

  printf("  OK.\n");

  g_udoip_comm.Init();
  printf("UDOIP Slave listening at port %u ...\n", g_udoip_comm.port);

	prepare_udoip_wait(g_udoip_comm.fdsocket);

  printf("Starting main cycle.\n");

  while (true)
  {
		g_udoip_comm.Run();

		// Do not use 100 % CPU time, especially on single core systems !
		// Do not wait long because the scope data must be written right on time
		#if 1
  		wait_for_udoip_ms(1);  // Wait for UDO-IP requests for 1 ms
		#else
			usleep(1000);  // sleep for 1 ms
		#endif
  }

  printf("so far so good.\n");
	return 0;
}

