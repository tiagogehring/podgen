#include <fstream>
#include <filesystem>

#include "PodGenerator.hpp"

namespace fs = std::filesystem;

/// Resolve relative path removing .. directory parts
fs::path resolvePath(fs::path from, const fs::path& path) {
    for (auto& f : path) {
        if (f == "..") {
            from = from.parent_path();
        } else {
            from /= f;
        }
    }

    return from;
}

static fs::path canonPath(std::string_view path) {
    std::string ret = fs::canonical(path).string();
    std::replace(ret.begin(), ret.end(), '\\', '/');
    return fs::path{ ret };
}

int main(int argc, char** argv) {    
    if (argc <= 1) {
        std::cerr << "Usage: podgen [schemas...] -t template_dir -o output_dir -c capnp_root_dir" << std::endl;
        return 1;
    }

    try
    {
        fs::path templateRoot;
        fs::path outputRoot;
        fs::path capnpRoot;
        std::vector<std::string> inputs;

        for (int i = 1; i < argc; i++)
        {
            if (i < argc - 1 && strcmp(argv[i], "-o") == 0)
            {
                outputRoot = canonPath(argv[++i]);
            }
            else if (i < argc - 1 && strcmp(argv[i], "-c") == 0)
            {
                if (!fs::exists(argv[++i]))
                {
                    std::cerr << "Invalid Cap'n Proto root: '" << argv[i] << "'";
                    return -1;
                }
                capnpRoot = canonPath(argv[i]);
            }
            else if (i < argc - 1 && strcmp(argv[i], "-t") == 0)
            {
                if (!fs::exists(argv[++i]))
                {
                    std::cerr << "Invalid template path: '" << argv[i] << "'";
                    return -1;
                }
                templateRoot = canonPath(argv[i]);
            }
            else
            {
                inputs.emplace_back(argv[i]);
            }
        }

        auto fs = kj::newDiskFilesystem();
        auto capnpImportPath = fs->getRoot().openSubdir(kj::Path::parse(capnpRoot.string()));
        auto templateImportPath = fs->getRoot().openSubdir(kj::Path::parse(templateRoot.string()));

        const kj::ReadableDirectory* paths[2] = { capnpImportPath.get(), templateImportPath.get() };
        kj::ArrayPtr<const kj::ReadableDirectory*> importPaths(paths, 2);

        PodGenStream dummy(std::cout, "", {});

        for (auto& input : inputs)
        {
            fs::path capnpFile = fs::current_path() / input;
            if (!fs::exists(capnpFile))
            {
                std::cout << "capnp file not found: " << capnpFile << std::endl;
                continue;
            }

            if (capnpFile.extension() != ".capnp")
            {
                std::cout << "not a capnp file: " << capnpFile << std::endl;
                continue;
            }

            std::cout << "parsing " << capnpFile.string() << std::endl;

            auto podConvertSource = outputRoot / capnpFile;
            podConvertSource.replace_extension(".convert.cpp");

            auto podHeader = outputRoot / capnpToPodImport(capnpFile);
            auto podInclude = podHeader.filename();

            auto podConvertHeader = outputRoot / capnpToConvertImport(capnpFile);
            auto podConvertInclude = podConvertHeader.filename();

            SchemaInfo info;

            auto putType = [&info](PodGenStream&, ::capnp::StructSchema schema, ::capnp::Schema parent) {
                info.internalTypesByName.emplace(schema.getProto().getDisplayName(), schema);
                info.internalTypesById.emplace(schema.getProto().getId(), schema);
                info.schemaParentOf.emplace(schema.getProto().getId(), parent.getProto().getId());
            };

            ::capnp::SchemaParser parser;
            info.schema = parser.parseFromDirectory(fs->getCurrent(), kj::Path::parse(kj::str(input.c_str())), importPaths);
            // info.schema = parser.parseFile()

            auto namesp = getNamespace(info.schema);
            if (!namesp.empty())
            {
                std::cout << "  found namespace " << namesp << std::endl;
            }
            info.importNamespaces.emplace(input, namesp);

            generateFromSchema(dummy, info.schema, putType);
            auto externalTypes = findExternalTypes(info.schema);
            info.externalTypes = std::move(externalTypes.typeMap);
            info.podHeaders = std::move(externalTypes.podHeaders);
            info.unions = findUnionFields(info.schema);

            auto parentPath = capnpFile.parent_path();
            auto file = capnpFile.string();
            for (auto& [alias, import] : getImportsFromCapnp(file))
            {
                fs::path p = capnpFile.parent_path();

                if (import.rfind("/capnp/", 0) == 0)
                {
                    continue;
                }
                else if (import[0] == '/')
                {
                    p = import.substr(1);
                }
                else
                {
                    p = resolvePath(p, import);
                }
                p = fs::relative(p);

                try
                {
                    std::cout << p << std::endl;
                    auto importSchema = parser.parseFromDirectory(fs->getCurrent(), kj::Path::parse(kj::str(p.string().c_str())), importPaths);
                    auto ns = getNamespace(importSchema);
                    std::cout << "  parsed import " << import;
                    if (!ns.empty())
                    {
                        std::cout << " with namespace " << ns;
                    }
                    std::cout << std::endl;

                    auto countTypes = generateFromSchema(dummy, importSchema, putType);
                    if (countTypes > 0)
                    {
                        info.importNamespaces.emplace(p.string(), ns);

                        // stick the import's using alias in the map with the same namespace
                        if (!alias.empty())
                        {
                            info.importAliases.emplace(alias, p.string());
                        }
                    }

                    externalTypes = findExternalTypes(importSchema);
                    info.externalTypes.merge(externalTypes.typeMap);
                    info.unions.merge(findUnionFields(importSchema));
                }
                catch (kj::Exception& e)
                {
                    std::cout << "ignoring import exception: " << e.getDescription().cStr() << std::endl;
                }
            }

            auto generate = [&](const fs::path& tmpl, const fs::path& dest) {
                std::ifstream in(tmpl);
                if (!in.good())
                {
                    std::cout << "error loading template " << tmpl << ": " << strerror(errno) << std::endl;
                    return false;
                }

                fs::create_directories(dest.parent_path());
                std::ofstream out(dest);
                if (!out.good())
                {
                    std::cout << "error writing to " << dest << ": " << strerror(errno) << std::endl;
                    return false;
                }

                std::cout << "  generating file " << dest << std::endl;
                generateFile(in, out, info, capnpFile);
                return true;
            };

            // pod header
            fs::path templatePath = templateRoot;
            bool result = generate(templatePath / "pod.hpp.tmpl", podHeader) && generate(templatePath / "pod_convert.hpp.tmpl", podConvertHeader)
                          && generate(templatePath / "pod_convert.cpp.tmpl", podConvertSource);

            if (!result)
            {
                return 1;
            }
        }
    }
    catch (kj::Exception ex)
    {
        std::cerr << ex.getDescription().cStr();
    }
    catch (std::exception ex)
    {
        std::cerr << ex.what();
    }

    return 0;
}
