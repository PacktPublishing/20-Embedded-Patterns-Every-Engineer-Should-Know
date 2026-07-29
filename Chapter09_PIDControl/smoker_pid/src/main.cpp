#include "Simulation.h"

#include <exception>
#include <fstream>
#include <iostream>
#include <string>

int main(int argc, char* argv[])
{
    if (argc < 2 || argc > 3)
    {
        std::cerr << "Usage: " << argv[0]
                  << " open-loop|p|pi|pid|windup|disturbance [output.csv]\n";
        return 1;
    }

    try
    {
        const auto scenario = parseScenario(argv[1]);
        Simulation simulation{scenario};

        if (argc == 3)
        {
            std::ofstream output{argv[2]};
            if (!output)
            {
                std::cerr << "Unable to open output file: " << argv[2] << '\n';
                return 1;
            }
            simulation.run(output);
            std::cout << "Wrote " << scenarioName(scenario) << " results to "
                      << argv[2] << '\n';
        }
        else
        {
            simulation.run(std::cout);
        }
    }
    catch (const std::exception& error)
    {
        std::cerr << "Error: " << error.what() << '\n';
        return 1;
    }
}
