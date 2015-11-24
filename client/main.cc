#include "client.hh"
#include "client2.hh"

int new_main(int argc, char *argv[])
{
  Client client{argc, argv};
  client.run();
	return EXIT_SUCCESS;
}

int main(int argc, char *argv[])
{
  old_main(argc, argv);
}
