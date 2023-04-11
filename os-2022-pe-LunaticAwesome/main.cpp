#include <array>
#include <iostream>
#include <fstream>
#include <cstring>
#include <vector>

template<size_t N>
size_t create_val(std::array<char, N> &arr, size_t pos) {
  return *reinterpret_cast<uint32_t *>(&arr[pos]);
}

size_t get_pos(std::ifstream &fin) {
  fin.seekg(0x3C);
  std::array<char, 4> data;
  fin.read(data.begin(), 4);
  return create_val(data, 0);
}

size_t get(std::ifstream &fin, size_t rva_address, size_t signature) {
  fin.seekg(signature + 24 + 240);
  size_t import_raw = 0;
  while (true) {
    std::array<char, 40> buf;
    fin.read(buf.begin(), 40);
    size_t section_virtual_size = create_val(buf, 8);
    size_t section_rva = create_val(buf, 12);
    if (section_rva <= rva_address
        && rva_address <= section_rva + section_virtual_size) {
      size_t section_raw = create_val(buf, 20);
      import_raw = section_raw + rva_address - section_rva;
      break;
    }
  }
  return import_raw;
}

void print_cstring(std::ifstream &fin) {
  char ch;
  fin >> ch;
  while (ch != '\0') {
    std::cout << ch;
    fin >> ch;
  }
  std::cout << std::endl;
}

uint32_t read_uint32(std::ifstream &fin) {
  std::array<char, 4> val;
  fin.read(val.begin(), 4);
  return create_val(val, 0);
}

uint32_t read_uint32(std::ifstream &fin, size_t position) {
  fin.seekg(position);
  return read_uint32(fin);
}

int main(int argc, char *argv[]) {
  if (argc != 3) {
    std::cerr << "invalid usage of pe-parser\n";
    return 2;
  }
  std::ifstream fin;
  try {
    fin.open(argv[2], std::ios_base::binary);
  } catch (...) {
    std::cerr << "file not found\n";
    return 2;
  }
  const size_t signature = get_pos(fin);
  if (std::strcmp(argv[1], "is-pe") == 0) {
    fin.seekg(signature);
    std::array<char, 4> buf;
    fin.read(buf.begin(), 4);
    if (buf == std::array < char, 4 > {'P', 'E', '\0', '\0'}) {
      std::cout << "PE" << std::endl;
      return 0;
    } else {
      std::cout << "Not PE" << std::endl;
      return 1;
    }
  } else if (std::strcmp(argv[1], "import-functions") == 0) {
    size_t import_table_rva = read_uint32(fin, signature + 24 + 0x78);
    size_t import_raw = get(fin, import_table_rva, signature);
    std::array<char, 20> data;
    fin.seekg(import_raw);
    std::vector <std::pair<uint32_t, uint32_t>> positions;
    while (true) {
      fin.read(data.begin(), 20);
      if (data == std::array < char, 20 > {}) {
        break;
      }
      positions.push_back({create_val(data, 12), create_val(data, 0)});
    }
    std::vector <size_t> dll_name_pos;
    for (size_t i = 0; i < positions.size(); i++) {
      dll_name_pos.push_back(get(fin, positions[i].first, signature));
    }
    for (size_t i = 0; i < dll_name_pos.size(); i++) {
      fin.seekg(dll_name_pos[i]);
      print_cstring(fin);
      fin.seekg(get(fin, positions[i].second, signature));
      std::vector <size_t> func_name_rva;
      while (true) {
        std::array<char, 8> data;
        fin.read(data.begin(), 8);
        if (data == std::array < char, 8 > {}) {
          break;
        }
        if ((data[7] & 0x80) == 0) {
          data[3] &= ~0x80;
          func_name_rva.push_back(create_val(data, 0));
        }
      }
      for (size_t j = 0; j < func_name_rva.size(); j++) {
        fin.seekg(get(fin, func_name_rva[j], signature) + 2);
        std::cout << "    ";
        print_cstring(fin);
      }
    }
  } else if (std::strcmp(argv[1], "export-functions") == 0) {
    size_t export_rva = read_uint32(fin, signature + 24 + 0x70);
    size_t export_raw = get(fin, export_rva, signature);
    std::array<char, 40> data;
    fin.seekg(export_raw);
    fin.read(data.begin(), 40);
    size_t name_pointer_rva = create_val(data, 32);
    size_t number_of_name_pointers = create_val(data, 24);
    fin.seekg(get(fin, name_pointer_rva, signature));
    std::vector <size_t> pointers_rva(number_of_name_pointers);
    for (size_t i = 0; i < number_of_name_pointers; i++) {
      pointers_rva[i] = read_uint32(fin);
    }
    for (size_t i = 0; i < pointers_rva.size(); i++) {
      fin.seekg(get(fin, pointers_rva[i], signature));
      print_cstring(fin);
    }
  }
}
