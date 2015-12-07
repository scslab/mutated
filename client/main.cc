#include "client.hh"

/**
 * Main method -- launch mutated.
 */
int main(int argc, char *argv[])
{
  Client client{argc, argv};
  client_ = &client;
  client.run();
	return EXIT_SUCCESS;
}
