#include <exception>
#include <iostream>
#include <system_error>

#include "client.hh"

/**
 * Main method -- launch mutated.
 */
int main(int argc, char *argv[])
{
    try {
        Client client{argc, argv};
        client.run();
    } catch (const std::system_error &e) {
        std::cerr << "System Error: " << e.what() << std::endl;
        std::cerr << " - Code: " << e.code().value() << std::endl;
        std::cerr << " - Category: " << e.code().category().name()
                  << std::endl;
        throw;
    } catch (const std::exception &e) {
        std::cerr << "Error: " << e.what() << std::endl;
        exit(EXIT_FAILURE);
    }
    return EXIT_SUCCESS;
}
