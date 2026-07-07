#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <VarjoToolkit/Core/VarjoSession.hpp>
#include <VarjoToolkit/Rendering/VarjoOcclusionMesh.hpp>

#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>

namespace fs = std::filesystem;

namespace {

struct Options {
    fs::path outputDirectory = "varjo_occlusion_mesh_dump";
    varjo_WindingOrder windingOrder = varjo_WindingOrder_Clockwise;
    bool writeCsv = true;
    bool writeObj = true;
    bool help = false;
};

void printUsage()
{
    std::cout
        << "VarjoOcclusionMeshDumpSample\n"
        << "\n"
        << "Usage:\n"
        << "  VarjoOcclusionMeshDumpSample.exe [options]\n"
        << "\n"
        << "Options:\n"
        << "  --out <dir>       Output directory. Default: varjo_occlusion_mesh_dump\n"
        << "  --winding <mode>  Triangle winding: cw, clockwise, ccw, counterclockwise. Default: cw\n"
        << "  --no-csv          Do not write CSV vertex dumps\n"
        << "  --no-obj          Do not write OBJ triangle dumps\n"
        << "  --help            Show this message\n";
}

bool parseWinding(const std::string& text, varjo_WindingOrder& winding)
{
    if (text == "cw" || text == "clockwise") {
        winding = varjo_WindingOrder_Clockwise;
        return true;
    }
    if (text == "ccw" || text == "counterclockwise") {
        winding = varjo_WindingOrder_CounterClockwise;
        return true;
    }
    return false;
}

bool parseArguments(int argc, char** argv, Options& options)
{
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--help" || arg == "-h") {
            options.help = true;
            return true;
        }
        if (arg == "--out") {
            if (i + 1 >= argc) {
                std::cerr << "Missing value for --out\n";
                return false;
            }
            options.outputDirectory = argv[++i];
            continue;
        }
        if (arg == "--winding") {
            if (i + 1 >= argc) {
                std::cerr << "Missing value for --winding\n";
                return false;
            }
            if (!parseWinding(argv[++i], options.windingOrder)) {
                std::cerr << "Invalid --winding value. Use cw or ccw.\n";
                return false;
            }
            continue;
        }
        if (arg == "--no-csv") {
            options.writeCsv = false;
            continue;
        }
        if (arg == "--no-obj") {
            options.writeObj = false;
            continue;
        }
        std::cerr << "Unknown option: " << arg << "\n";
        return false;
    }
    return true;
}

std::string windingName(varjo_WindingOrder winding)
{
    return winding == varjo_WindingOrder_CounterClockwise ? "counterclockwise" : "clockwise";
}

void writeCsv(const fs::path& path, int32_t viewIndex, const VarjoOcclusionMesh::Snapshot& snapshot)
{
    std::ofstream out(path);
    if (!out) {
        throw std::runtime_error("failed to open CSV output: " + path.string());
    }

    out << "view_index,vertex_index,triangle_index,triangle_vertex_index,x,y\n";
    out << std::fixed << std::setprecision(9);
    for (size_t i = 0; i < snapshot.vertices.size(); ++i) {
        const auto& v = snapshot.vertices[i];
        out << viewIndex << ','
            << i << ','
            << (i / 3) << ','
            << (i % 3) << ','
            << v.x << ','
            << v.y << '\n';
    }
}

void writeObj(const fs::path& path, int32_t viewIndex, varjo_WindingOrder winding, const VarjoOcclusionMesh::Snapshot& snapshot)
{
    std::ofstream out(path);
    if (!out) {
        throw std::runtime_error("failed to open OBJ output: " + path.string());
    }

    out << "# Varjo occlusion mesh dump\n";
    out << "# view_index " << viewIndex << "\n";
    out << "# winding " << windingName(winding) << "\n";
    out << "# vertex_count " << snapshot.vertices.size() << "\n";
    out << "# triangle_count " << (snapshot.vertices.size() / 3) << "\n";
    out << std::fixed << std::setprecision(9);

    for (const auto& v : snapshot.vertices) {
        out << "v " << v.x << ' ' << v.y << " 0\n";
    }

    out << "\n";
    const size_t triangleCount = snapshot.vertices.size() / 3;
    for (size_t tri = 0; tri < triangleCount; ++tri) {
        const size_t base = tri * 3 + 1;
        out << "f " << base << ' ' << (base + 1) << ' ' << (base + 2) << "\n";
    }

    const size_t remainder = snapshot.vertices.size() % 3;
    if (remainder != 0) {
        out << "# Warning: ignored trailing vertex count " << remainder << " because Varjo occlusion mesh is expected to be a triangle list.\n";
    }
}

void writeSummary(const fs::path& path, int32_t viewCount, varjo_WindingOrder winding)
{
    std::ofstream out(path);
    if (!out) {
        throw std::runtime_error("failed to open summary output: " + path.string());
    }
    out << "Varjo occlusion mesh dump\n";
    out << "view_count=" << viewCount << "\n";
    out << "winding=" << windingName(winding) << "\n";
    out << "topology=triangle_list\n";
}

} // namespace

int main(int argc, char** argv)
{
    Options options;
    if (!parseArguments(argc, argv, options)) {
        printUsage();
        return 1;
    }
    if (options.help) {
        printUsage();
        return 0;
    }
    if (!options.writeCsv && !options.writeObj) {
        std::cerr << "Nothing to write: both CSV and OBJ outputs are disabled.\n";
        return 1;
    }

    try {
        std::cout << "Initializing Varjo session...\n";
        VarjoSession session;
        if (!session) {
            std::cerr << "Varjo session initialization failed: " << session.lastError() << "\n";
            return 1;
        }

        const int32_t viewCount = session.viewCount();
        if (viewCount <= 0) {
            std::cerr << "Varjo runtime returned no views.\n";
            return 1;
        }

        fs::create_directories(options.outputDirectory);
        writeSummary(options.outputDirectory / "summary.txt", viewCount, options.windingOrder);

        std::cout << "Dumping occlusion meshes. views=" << viewCount
                  << " winding=" << windingName(options.windingOrder)
                  << " out=" << options.outputDirectory.string() << "\n";

        for (int32_t viewIndex = 0; viewIndex < viewCount; ++viewIndex) {
            VarjoOcclusionMesh mesh(session.shared(), viewIndex, options.windingOrder);
            if (!mesh) {
                std::cerr << "Failed to create occlusion mesh for view " << viewIndex << ": " << mesh.lastError() << "\n";
                continue;
            }

            const auto snapshot = mesh.snapshot();
            if (!snapshot.valid) {
                std::cerr << "Invalid occlusion mesh snapshot for view " << viewIndex << "\n";
                continue;
            }

            std::cout << "view=" << viewIndex
                      << " vertexCount=" << snapshot.vertices.size()
                      << " triangleCount=" << (snapshot.vertices.size() / 3) << "\n";

            const std::string prefix = "view" + std::to_string(viewIndex);
            if (options.writeCsv) {
                writeCsv(options.outputDirectory / (prefix + "_vertices.csv"), viewIndex, snapshot);
            }
            if (options.writeObj) {
                writeObj(options.outputDirectory / (prefix + "_mesh.obj"), viewIndex, options.windingOrder, snapshot);
            }
        }

        std::cout << "Done.\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << "\n";
        return 1;
    }
}
