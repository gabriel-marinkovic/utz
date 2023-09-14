#include <stdio.h>
#include <stdlib.h>
#include "../utz.h"

#include <string>
#include <iostream>
#include <fstream>
#include <vector>

std::vector<char> readFileToVector(const std::string& filename) {
    // Open the file in binary mode
    std::ifstream file(filename, std::ios::binary | std::ios::ate);

    // Check if the file was opened successfully
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open file: " + filename);
    }

    // Determine the size of the file
    std::streamsize size = file.tellg();

    // Set the position of the next character to be extracted from the input stream to the beginning
    file.seekg(0, std::ios::beg);

    // Resize the vector to hold all the bytes in the file
    std::vector<char> buffer(size);

    // Read the entire file into the vector
    if (!file.read(buffer.data(), size)) {
        throw std::runtime_error("Failed to read file: " + filename);
    }

    return buffer;
}

int main(int argc, char** argv)
{
    std::vector<char> file = readFileToVector("tzdata2023c.tar.gz");

    utz_timezones tzs;
    int result = utz_parse_iana_tzdb_targz(&tzs, file.data(), (int)file.size());
    if (!result)
    {
        printf("ERROR: %s", tzs.parsing_error);
        return 1;
    }

    unsigned long long cca_size = 0;
    for (int i = 0; i < tzs.timezone_count; i++)
    {
        printf("TZ: %s\n", tzs.timezones[i].name);

        cca_size += tzs.timezones[i].range_count * sizeof(utz_time_range);
        for (int j = 0; j < tzs.timezones[i].range_count; j++)
        {
            auto& r = tzs.timezones[i].ranges[j];
            printf("\tsince: %lld\toffset: %d\tabbr: %s\n", r.since, r.offset_seconds, r.zone_abbreviation);
        }
    }

    printf("CCA SIZE: %llu\n", cca_size);

    utz_free_timezones(&tzs);
    return result ? 0 : 1;
}
