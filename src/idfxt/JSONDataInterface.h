// copyright 2014, Shannon Mackey <mackey@BUILDlab.net>
#ifndef JSONDATAINTERFACE_H
#define JSONDATAINTERFACE_H

#include <string>

extern "C"
{
#include "../../third_party/cJSON/cJSON.h"
}

namespace idfx {

class JSONDataInterface
{
public:
    JSONDataInterface(const std::string &json_schema);
    ~JSONDataInterface();

    cJSON *getSchemaObject(const std::string &object_type);
    cJSON *getModelRootObject();

    bool importModel(const std::string &json_content);
    bool integrateModel();
    void writeJSONdata(const std::string &filename);

private:
    cJSON *schema_j;
    cJSON *model_j;

    bool validateModel();
    void checkRange(cJSON *attribute, const std::string &property_name, const std::string &child_name, bool &valid, double property_value);
    void checkNumeric(double property_value, const std::string &property_name, cJSON *schema_object, bool &valid, const std::string &child_name);
};

} //idfxt namespace
#endif // JSONDATAINTERFACE_H
