#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <algorithm>
#include <map>

const std::string whitespace = " \t\n";

std::vector<std::string> generic_literals;
std::vector<std::string> generic_values;
std::vector<std::string> port_literals;
std::vector<std::string> io_type;
std::vector<std::string> vhdl_type;
std::vector<std::string> initial_value;
std::string entity_name;
std::string tb_name;

enum reserved {
	library,
	use,
	entity,
	generic,
	port,
	end,
	delimiter,
	unknown
};

static std::map<std::string, reserved> keyword_map;

// Template to return default value if key is not found in map
template <typename K, typename V>
V GetWithDef (const std::map<K, V>& m, const K& key) {
	
	typename std::map<K, V>::const_iterator it = m.find( key );
	if (it == m.end()) return unknown;
	
	return it->second;
}

void init_map () {
	keyword_map["LIBRARY"] = library;
	keyword_map["USE"] = use;
	keyword_map["ENTITY"] = entity;
	keyword_map["GENERIC"] = generic;
	keyword_map["PORT"] = port;
	keyword_map["END"] = end;
	keyword_map[");"] = delimiter;
}	

int main (int argc, char** argv) {
	
	if (argc < 2 || argc > 3) {
		std::cout << "Usage: tbgen.exe {SOURCE} [TARGET]" << std::endl;
		return 0;
	}
	
	std::fstream fi, fo;
	fi.open(argv[1], std::fstream::in);
	int generic_missing_values = 0;

	init_map();

	if (fi.good()) {
		
		bool found_entity = false;
		bool found_entity_end = false;
		bool found_library = false;
		bool found_generic = false;
		bool found_generic_end = false;
		bool found_port = false;
		bool found_port_end = false;

		int lib_begin_line, lib_end_line;
		int entity_begin_line, entity_end_line;
		int generic_begin_line, generic_end_line;
		int port_begin_line, port_end_line;

		std::string line;
		std::string keyword;
		std::string complement;
		std::string::size_type pos;
		

		for (int n = 1; std::getline(fi, line) && !found_entity_end; n++) {
	
			/* Trimming */
			pos = line.find_first_not_of(whitespace);
			line.erase(0, pos);
			
			if ((line[0] == '-' && line[1] == '-') || line.empty()) continue; // Skip comments
			pos = line.find_first_of(whitespace);
			keyword = line.substr(0, pos);

			pos = keyword.find_first_of("(");
			if (pos != std::string::npos) keyword.erase(pos);

			/* To Uppercase */
			std::transform(keyword.begin(), keyword.end(), keyword.begin(), toupper);
			switch (GetWithDef(keyword_map, keyword)) {
				case library:
					if (!found_library) {
						found_library = true;
						lib_begin_line = n;
						lib_end_line = n;
					}
					break;
				case use:
					lib_end_line = n;
					break;
				case entity:
					pos = line.find_first_of(whitespace);
					line.erase(0, pos);
					pos = line.find_first_not_of(whitespace);
					line.erase(0, pos);
					pos = line.find_first_of(whitespace);
					entity_name = line.substr(0, pos);	
					found_entity = true;
					entity_begin_line = n;
					break;
				case generic:
					found_generic = true;
					generic_begin_line = n;
					break;
				case port:
					found_port = true;
					port_begin_line = n;
					break;
				case end:
					found_entity_end = true;
					entity_end_line = n;	
					break;
				case delimiter:
					{
						int aux = found_generic << 3 | found_generic_end << 2 | found_port << 1 | found_port_end;
						switch (aux) {
							case 0b1000:
							case 0b1011:
								found_generic_end = true;
								generic_end_line = n;
								break;
							case 0b0010:
							case 0b1110:
								found_port_end = true;
								port_end_line = n;
								break;
							default:
								break;
						}
					}
					break;	
				default:
					{
						bool line_ended = false;
						bool name_ended = false;
						int names_number = 0;

						std::string port_name;
						std::string generic_name;
						std::string io;
						std::string type;
						std::string value;
						
						if (found_port && !found_port_end) {
							
							names_number = 0;
							// Parse and save port names
							while (!name_ended) {
							
								pos = line.find_first_of(",:");
								if (line[pos] == ':') name_ended = true;

								// Split strings
								port_name = line.substr(0, pos);
								line.erase(0, pos + 1);

								// Trim port name
								pos = port_name.find_first_not_of(whitespace);
								port_name.erase(0, pos);
								
								pos = port_name.find_first_of(whitespace);
								if (pos != std::string::npos) port_name.erase(pos);
								
								port_literals.push_back(port_name);
								names_number++;
							}

							// Determine IO type
							pos = line.find_first_not_of(whitespace);  	
							line.erase(0, pos);
							pos = line.find_first_of(whitespace);
							io = line.substr(0, pos);
							
							line.erase(0, pos);
							for (int i = 0; i < names_number; i++) io_type.push_back(io);
						
							// Determine VHDL type
							pos = line.find_first_not_of(whitespace);
							line.erase(0, pos);

							pos = line.find_first_of(whitespace + "(;");
							if (line[pos] == '(') pos = line.find_first_of(")") + 1;
							type = line.substr(0, pos);
							line.erase(0, pos);
								
							for (int i = 0; i < names_number; i++) vhdl_type.push_back(type);

							// Determine default value, if any
							pos = line.find_first_not_of(whitespace);

							if (line[pos] == ':') { // Finds possible default value
								line.erase(0, pos + 2);
								pos = line.find_first_not_of(whitespace);
								line.erase(0, pos);

								pos = line.find_first_of(whitespace);
								if (line[0] == '(') pos = line.find_first_of(")") + 1;
								
								value = line.substr(0, pos);
								line.erase(0, pos);

								for (int i = 0; i < names_number; i++) initial_value.push_back(value);

							} else if (line[pos] == ';' || pos == std::string::npos) {
								for (int i = 0; i < names_number; i++) initial_value.push_back(std::string("NO_VAL"));
							}
							
						} else if (found_generic && !found_generic_end) {	
							
							names_number = 0;
							// Parse and save generic names
							while (!name_ended) {
							
								pos = line.find_first_of(",:");
								if (line[pos] == ':') name_ended = true;

								// Split strings
								generic_name = line.substr(0, pos);
								line.erase(0, pos + 1);

								// Trim generic name
								pos = generic_name.find_first_not_of(whitespace);
								generic_name.erase(0, pos);
								
								pos = generic_name.find_first_of(whitespace);
								if (pos != std::string::npos) generic_name.erase(pos);
								
								generic_literals.push_back(port_name);
								names_number++;
							}
						
							// find and ignore generic type
							pos = line.find_first_not_of(whitespace);
							line.erase(0, pos);

							pos = line.find_first_of(whitespace + "(;");
							if (line[pos] == '(') pos = line.find_first_of(")") + 1;
							line.erase(0, pos);

							// Determine default value, if any
							pos = line.find_first_not_of(whitespace);

							if (line[pos] == ':') { // Finds possible default value
								line.erase(0, pos + 2);
								pos = line.find_first_not_of(whitespace);
								line.erase(0, pos);

								pos = line.find_first_of(whitespace);
								if (line[0] == '(') pos = line.find_first_of(")") + 1;
								
								value = line.substr(0, pos);
								line.erase(0, pos);

								for (int i = 0; i < names_number; i++) generic_values.push_back(value);

							} else if (line[pos] == ';' || pos == std::string::npos) {
								for (int i = 0; i < names_number; i++) {
									generic_values.push_back(std::string("NO_VAL"));
									generic_missing_values++;
								}
							}
						
						}	
					}
					
					break;
			}

		}		

		// Start writing new file
		
		fi.clear();
		fi.seekg(0, std::fstream::beg);

		std::string outfile_name;
		if (argc == 3) {
			outfile_name = std::string(argv[2]);
			pos = outfile_name.find_first_of(".");
			tb_name = outfile_name.substr(0, pos);
		}
		else {
			tb_name = entity_name + "_tb";
			outfile_name = tb_name + ".vhd";
		}

		fo.open(outfile_name, std::ios::in);
		if (fo.good())
		{
			// Check if output file already exists and
			// if so, asks if it should be overwritten
			if (fo.peek() != std::fstream::traits_type::eof())
			{
				std::string user_input;
				bool ovr = false;
				bool valid = false;
				do 
				{
					std::cout << "Output file " << outfile_name << " already exists and is not empty. Do you want to overwride it? (Y/N) ";
					std::getline(std::cin, user_input);
					if (user_input == "Y" || user_input == "y")
					{
						ovr = true;
						valid = true;
					} 
					else if (user_input == "N" || user_input == "n") valid = true;
				} while (!valid);
				
				if (!ovr)
				{
					std::cout << "Testbench could not be generated." << std::endl;
					return 0;
				}
				 
				fo.clear();
			}
		}
		fo.close();

		fo.open(outfile_name, std::ios::out);
		if (fo.good()) {
			

			// Copy libraries
			for (int n = 1; n < lib_begin_line; n++) getline(fi, line);
			for (int n = lib_begin_line; n <= lib_end_line; n++) {
				getline(fi, line);
				fo << line << "\n";
			}
						
			// tb entity declaration
			fo << "\n";
			fo << "entity " << tb_name << " is\nend entity;\n";

			// Architecture of tb
			fo << "\narchitecture behavior of " << tb_name << " is\n";
			
			// Component declaration
			fo << "\n\tcomponent " << entity_name << " is\n";			
			for (int n = lib_end_line + 1; n <= entity_begin_line; n++) getline(fi, line);
			for (int n = entity_begin_line; n < entity_end_line - 1; n++) {
				getline(fi, line);
				fo << "\t" << line << "\n";
			}
			fo << "\tend component;\n\n";

			// Signal declaration
			for (int i = 0; i < port_literals.size(); i++) {
				fo << "\tsignal " << port_literals[i] << ": " << vhdl_type[i];
				if (initial_value[i] != "NO_VAL") fo << " := " << initial_value[i];
				fo << ";\n";
			}

			// Begin architecture
			fo << "\nbegin\n\n\tuut: " << entity_name << " ";
			if (found_generic) {
				fo << "generic map (\n";
				for (int i = 0; i < generic_literals.size() - 1; i++) fo << "\t\t" << generic_literals[i] << " => " << generic_values[i] << ",\n";
				fo << "\t\t" << generic_literals[generic_literals.size() - 1] << " => " << generic_values[generic_values.size() - 1] << "\n\t) ";
			}

			fo << "port map (\n";
			for (int i = 0; i < port_literals.size() - 1; i++) fo << "\t\t" << port_literals[i] << " => " << port_literals[i] << ",\n";
			fo << "\t\t" << port_literals[port_literals.size() - 1] << " => " << port_literals[port_literals.size() - 1] << "\n\t);";
			
			fo << "\n\n end architecture;";

		} else {
			std::cout << "Couldn't open output file: " << outfile_name << std::endl;
			return 0;
		}

	} else {
		std::cout << "Couldn't open input file: " << argv[1];
		return 0;
	}

	if (generic_missing_values > 0) std::cout << "Warning: " << generic_missing_values << " generic literal(s) without value; manual input needed\n";
	std::cout << "Successfully generated test bench file: " << tb_name << ".vhd\n";

//	for (int i = 0; i < port_literals.size(); i++) std::cout << port_literals[i] << ": " << io_type[i] << " " << vhdl_type[i] << " := " << initial_value[i] << std::endl;
	return 0;
}	
