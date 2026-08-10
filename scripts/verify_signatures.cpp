#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

struct signature
{
  std::string name;
  std::vector<int> bytes;
};

struct module
{
  std::string path;
  std::vector<std::uint8_t> bytes;
};

static bool read_file(const std::string& path, std::vector<std::uint8_t>& bytes)
{
  std::ifstream input(path, std::ios::binary | std::ios::ate);
  if (!input)
  {
    return false;
  }

  const std::streamsize size = input.tellg();
  if (size <= 0 || static_cast<std::uintmax_t>(size) > std::numeric_limits<std::size_t>::max())
  {
    return false;
  }

  bytes.resize(static_cast<std::size_t>(size));
  input.seekg(0, std::ios::beg);
  return static_cast<bool>(input.read(reinterpret_cast<char*>(bytes.data()), size));
}

static bool parse_hex_byte(const std::string& token, int& value)
{
  if (token.size() != 2)
  {
    return false;
  }

  std::istringstream stream(token);
  stream >> std::hex >> value;
  return !stream.fail() && stream.eof() && value >= 0 && value <= 0xff;
}

static bool parse_signature_line(const std::string& line, signature& result)
{
  constexpr const char* prefix = "constexpr const char* ";
  const std::size_t prefix_length = std::string(prefix).size();
  if (line.compare(0, prefix_length, prefix) != 0)
  {
    return false;
  }

  const std::size_t equals = line.find('=', prefix_length);
  const std::size_t quote_start = line.find('"', equals == std::string::npos ? prefix_length : equals);
  const std::size_t quote_end = line.rfind('"');
  if (equals == std::string::npos || quote_start == std::string::npos || quote_end <= quote_start)
  {
    return false;
  }

  result.name = line.substr(prefix_length, equals - prefix_length);
  while (!result.name.empty() && result.name.back() == ' ')
  {
    result.name.pop_back();
  }

  std::istringstream pattern_stream(line.substr(quote_start + 1, quote_end - quote_start - 1));
  std::string token;
  while (pattern_stream >> token)
  {
    if (token == "?" || token == "??")
    {
      result.bytes.push_back(-1);
      continue;
    }

    int value = 0;
    if (!parse_hex_byte(token, value))
    {
      return false;
    }
    result.bytes.push_back(value);
  }

  return !result.name.empty() && !result.bytes.empty();
}

static bool load_signatures(const std::string& path, std::vector<signature>& signatures)
{
  std::ifstream input(path);
  if (!input)
  {
    return false;
  }

  std::string line;
  std::string pending;
  while (std::getline(input, line))
  {
    if (pending.empty())
    {
      if (line.compare(0, std::string("constexpr const char* ").size(), "constexpr const char* ") != 0)
      {
        continue;
      }
      pending = line;
    }
    else
    {
      pending += ' ';
      pending += line;
    }

    if (pending.find('"') == std::string::npos || pending.rfind('"') == pending.find('"'))
    {
      continue;
    }

    signature parsed;
    if (parse_signature_line(pending, parsed))
    {
      signatures.push_back(std::move(parsed));
    }
    pending.clear();
  }

  return !signatures.empty();
}

static std::size_t longest_fixed_prefix(const signature& pattern)
{
  std::size_t length = 0;
  while (length < pattern.bytes.size() && pattern.bytes[length] >= 0)
  {
    ++length;
  }
  return length;
}

static std::size_t count_matches(const signature& pattern, const module& target, std::vector<std::size_t>& offsets)
{
  const std::size_t pattern_size = pattern.bytes.size();
  if (pattern_size > target.bytes.size())
  {
    return 0;
  }

  const std::size_t fixed_prefix_size = longest_fixed_prefix(pattern);
  for (std::size_t offset = 0; offset + pattern_size <= target.bytes.size(); ++offset)
  {
    bool prefix_matches = true;
    for (std::size_t index = 0; index < fixed_prefix_size; ++index)
    {
      if (target.bytes[offset + index] != static_cast<std::uint8_t>(pattern.bytes[index]))
      {
        prefix_matches = false;
        break;
      }
    }
    if (!prefix_matches)
    {
      continue;
    }

    bool matches = true;
    for (std::size_t index = fixed_prefix_size; index < pattern_size; ++index)
    {
      if (pattern.bytes[index] >= 0 && target.bytes[offset + index] != static_cast<std::uint8_t>(pattern.bytes[index]))
      {
        matches = false;
        break;
      }
    }
    if (matches)
    {
      offsets.push_back(offset);
    }
  }

  return offsets.size();
}

static std::string basename(const std::string& path)
{
  const std::size_t slash = path.find_last_of('/');
  return slash == std::string::npos ? path : path.substr(slash + 1);
}

int main(int argc, char** argv)
{
  if (argc < 3)
  {
    std::cerr << "usage: verify_signatures <sigs.hpp> <module> [module ...]\n";
    return 2;
  }

  std::vector<signature> signatures;
  if (!load_signatures(argv[1], signatures))
  {
    std::cerr << "unable to load signatures from " << argv[1] << '\n';
    return 2;
  }

  std::vector<module> modules;
  for (int index = 2; index < argc; ++index)
  {
    module loaded;
    loaded.path = argv[index];
    if (!read_file(loaded.path, loaded.bytes))
    {
      std::cerr << "unable to read module " << loaded.path << '\n';
      return 2;
    }
    modules.push_back(std::move(loaded));
  }

  bool all_found = true;
  std::size_t total_matches = 0;
  std::cout << "signatures=" << signatures.size() << " modules=" << modules.size() << '\n';
  for (const signature& pattern : signatures)
  {
    std::size_t signature_matches = 0;
    std::cout << pattern.name << ':';
    for (const module& target : modules)
    {
      std::vector<std::size_t> offsets;
      const std::size_t matches = count_matches(pattern, target, offsets);
      if (matches == 0)
      {
        continue;
      }

      signature_matches += matches;
      total_matches += matches;
      std::cout << ' ' << basename(target.path) << '=' << matches << "(@0x" << std::hex << offsets.front() << std::dec << ')';
    }

    if (signature_matches == 0)
    {
      all_found = false;
      std::cout << " MISSING";
    }
    else if (signature_matches > 1)
    {
      std::cout << " TOTAL=" << signature_matches;
    }
    std::cout << '\n';
  }

  std::cout << "total_matches=" << total_matches << " result=" << (all_found ? "PASS" : "FAIL") << '\n';
  return all_found ? 0 : 1;
}
