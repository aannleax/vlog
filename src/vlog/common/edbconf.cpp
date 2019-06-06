#include <vlog/edbconf.h>

#include <kognac/logs.h>
#include <kognac/utils.h>

#include <trident/utils/tridentutils.h>

#include <iostream>
#include <fstream>
#include <sstream>

EDBConf::EDBConf(string rawcontent, bool isFile) {
    if (isFile) {
        LOG(INFOL) << "Parsing EDB configuration " << rawcontent;
        //Read the input file line by line
        string f = rawcontent;
        std::ifstream file(f);
        string line;
        rawcontent = "";
        while (std::getline(file, line)) {
            rawcontent += line + "\n";
        }
    }
    parse(rawcontent);
}

void EDBConf::setRootPath(std::string path) {
    this->rootPath = path;
}

std::string EDBConf::getRootPath() const {
    return rootPath;
}

void EDBConf::parse(string f) {
    std::stringstream reader(f);
    std::string line;
    while (std::getline(reader, line)) {
        if (line == "" || (line.size() > 0 && line[0] == '#')) {
            continue;
        }
        if (Utils::starts_with(line, "EDB")) {
            //Read the ID of the edb
            std::size_t found = line.find("_");
            if (found == string::npos) {
                LOG(ERRORL) << "Malformed line in edb.conf file: " << line;
                throw ("Malformed line: " + line);
            }

            string idedb = line.substr(3, found - 3);
            int id = TridentUtils::lexical_cast<int>(idedb);

            //Get the correspondent struct
            if (tables.size() < id + 1) {
                tables.resize(id + 1);
            }
            Table &table = tables[id];

            //Get type of parameter
            size_t idxAss = line.find("=");
            string typeParam = line.substr(found + 1, idxAss - found - 1);
            if (typeParam == "predname") {
                string predname = line.substr(idxAss + 1);
                table.predname = predname;
            } else if (typeParam == "type") {
                string typeStorage = line.substr(idxAss + 1);
                table.type = typeStorage;
            } else if (Utils::starts_with(typeParam, "param")) {
                //It's param...something
                int paramid = TridentUtils::lexical_cast<int>(typeParam.substr(5));
                if (table.params.size() <= paramid)
                    table.params.resize(paramid + 1);
                table.params[paramid] = line.substr(idxAss + 1);
            } else {
                //I don't know what it is. Throw error.
                LOG(ERRORL) << "Malformed line in edb.conf file: " << line;
                throw ("Malformed line: " + line);
            }
        } else {
            throw ("Malformed line: " + line);
        }
    }

#ifdef DEBUG
    for (const auto &table : tables) {
        string details = "conf edb table: predname=" + table.predname + " type=" + table.type;
        string params = " PARAMS: ";
        for (const auto p : table.params) {
            params += p + " ";
        }
        LOG(DEBUGL) << details << " " << params;
    }
#endif
}
