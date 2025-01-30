#include <core/system/CommandLine.hpp>

#include <core/logging/Logger.hpp>

#include <core/filesystem/FilePath.hpp>

#include <core/threading/Thread.hpp>
#include <core/threading/Scheduler.hpp>
#include <core/threading/AtomicVar.hpp>
#include <core/threading/TaskSystem.hpp>
#include <core/threading/TaskThread.hpp>
#include <core/threading/Mutex.hpp>

#include <core/memory/UniquePtr.hpp>

#include <driver/Driver.hpp>
#include <driver/clang/ClangDriver.hpp>

#include <generator/generators/CXXModuleGenerator.hpp>
#include <generator/generators/CSharpModuleGenerator.hpp>

#include <parser/Parser.hpp>

#include <analyzer/Analyzer.hpp>

namespace hyperion {
namespace buildtool {

HYP_DEFINE_LOG_CHANNEL(BuildTool);

class WorkerThread : public TaskThread
{
public:
    WorkerThread(ThreadID id)
        : TaskThread(id)
    {
    }

    virtual ~WorkerThread() override = default;
};

class WorkerThreadPool : public TaskThreadPool
{
public:
    WorkerThreadPool()
    {
        CreateThreads<WorkerThread>(NAME("BuildTool_WorkerThread"), 4);
    }

    virtual ~WorkerThreadPool() override = default;
};

class HypBuildTool
{
public:
    HypBuildTool(
        UniquePtr<IDriver> &&driver,
        const FilePath &working_directory,
        const FilePath &source_directory,
        const FilePath &cxx_output_directory,
        const FilePath &csharp_output_directory,
        const HashSet<FilePath> &exclude_directories
    ) : m_driver(std::move(driver))
    {
        m_analyzer.SetWorkingDirectory(working_directory);
        m_analyzer.SetSourceDirectory(source_directory);
        m_analyzer.SetCXXOutputDirectory(cxx_output_directory);
        m_analyzer.SetCSharpOutputDirectory(csharp_output_directory);

        m_analyzer.SetExcludeDirectories(exclude_directories);

        m_analyzer.SetGlobalDefines(GetGlobalDefines());
        m_analyzer.SetIncludePaths(GetIncludePaths());

        m_thread_pool.Start();
    }

    ~HypBuildTool()
    {
        m_thread_pool.Stop();
    }

    Result<void> Run()
    {
        FindModules();

        Task process_modules = ProcessModules();
        WaitWhileTaskRunning(process_modules);

        for (const UniquePtr<Module> &mod : m_analyzer.GetModules()) {
            for (const Pair<String, HypClassDefinition> &hyp_class : mod->GetHypClasses()) {
                HYP_LOG(BuildTool, Info, "Class: {}", hyp_class.first);

                // Log out all members
                for (const HypMemberDefinition &hyp_member : hyp_class.second.members) {
                    if (!hyp_member.cxx_type) {
                        continue;
                    }

                    json::JSONValue json;
                    hyp_member.cxx_type->ToJSON(json);

                    HYP_LOG(BuildTool, Info, "\tMember: {}\t{}", hyp_member.name, json.ToString(true));
                }
            }
        }

        Task generate_output_files = GenerateOutputFiles();
        WaitWhileTaskRunning(generate_output_files);

        HYP_LOG(BuildTool, Info, "Build tool finished");

        if (m_analyzer.GetState().HasErrors()) {
            for (const AnalyzerError &error : m_analyzer.GetState().errors) {
                HYP_LOG(BuildTool, Error, "Error: {}\t{}", error.GetMessage(), error.GetErrorMessage());
            }

            return HYP_MAKE_ERROR(Error, "Build tool finished with errors");
        }

        return { };
    }

private:
    HashMap<String, String> GetGlobalDefines() const
    {
        return {
            { "HYP_BUILDTOOL", "1" },
            { "HYP_VULKAN", "1" },
            { "HYP_CLASS(...)", "" },
            { "HYP_STRUCT(...)", "" },
            { "HYP_ENUM(...)", "" },
            { "HYP_FIELD(...)", "" },
            { "HYP_METHOD(...)", "" },
            { "HYP_PROPERTY(...)", "" },
            { "HYP_CONSTANT(...)", "" },
            { "HYP_OBJECT_BODY(...)", "" },
            { "HYP_API", "" },
            { "HYP_EXPORT", "" },
            { "HYP_IMPORT", "" },
            { "HYP_FORCE_INLINE", "inline" },
            { "HYP_NODISCARD", "" }
        };
    }

    HashSet<String> GetIncludePaths() const
    {
        const FilePath &working_directory = m_analyzer.GetWorkingDirectory();

        return {
            working_directory / "src",
            working_directory / "include"
        };
    }

    void FindModules()
    {
        HYP_LOG(BuildTool, Info, "Finding modules...");

        Proc<void, const FilePath &> IterateFilesAndSubdirectories;

        IterateFilesAndSubdirectories = [&](const FilePath &dir)
        {
            for (const String &exclude_dir : m_analyzer.GetExcludeDirectories()) {
                const FilePath exclude_dir_relative = FilePath(FileSystem::RelativePath(exclude_dir.Data(), m_analyzer.GetSourceDirectory().Data()).c_str());
                const FilePath relative_path = FilePath(FileSystem::RelativePath(dir.Data(), m_analyzer.GetSourceDirectory().Data()).c_str());

                if (relative_path.StartsWith(exclude_dir_relative)) {
                    HYP_LOG(BuildTool, Info, "Excluding directory: {}", dir);

                    return;
                }
            }

            Array<Module *> local_modules;
            
            for (const FilePath &file : dir.GetAllFilesInDirectory()) {
                if (file.EndsWith(".hpp")) {
                    local_modules.PushBack(m_analyzer.AddModule(file));
                }
            }

            Array<FilePath> local_subdirectories = dir.GetSubdirectories();

            HYP_LOG(BuildTool, Info, "On worker thread {}: {}: Discovered {} modules, {} subdirectories",
                ThreadID::Current().name, dir, local_modules.Size(), local_subdirectories.Size());

            if (local_subdirectories.Empty()) {
                return;
            }

            for (const FilePath &subdirectory : local_subdirectories) {
                IterateFilesAndSubdirectories(subdirectory);
            }
        };

        IterateFilesAndSubdirectories(m_analyzer.GetSourceDirectory());

        HYP_LOG(BuildTool, Info, "Found {} total modules", m_analyzer.GetModules().Size());
    }

    Task<void> ProcessModules()
    {
        HYP_LOG(BuildTool, Info, "Processing modules...");

        Task<void> task;

        TaskBatch *batch = new TaskBatch();
        batch->pool = &m_thread_pool;
        batch->OnComplete.Bind([batch, task_executor = task.Initialize()]()
        {
            task_executor->Fulfill();

            delete batch;
        }).Detach();

        for (const UniquePtr<Module> &mod : m_analyzer.GetModules()) {
            batch->AddTask([this, mod = mod.Get()]()
            {
                HYP_LOG(BuildTool, Info, "Processing module: {}", mod->GetPath());

                auto result = m_driver->ProcessModule(m_analyzer, *mod);

                if (result.HasError()) {
                    m_analyzer.AddError(result.GetError());

                    return;
                }
            });
        }

        TaskSystem::GetInstance().EnqueueBatch(batch);

        return task;
    }

    Task<void> GenerateOutputFiles()
    {
        HYP_LOG(BuildTool, Info, "Generating output files...");

        Task<void> task;

        TaskBatch *batch = new TaskBatch();
        batch->pool = &m_thread_pool;
        batch->OnComplete.Bind([batch, task_executor = task.Initialize()]()
        {
            task_executor->Fulfill();

            delete batch;
        }).Detach();

        RC<CXXModuleGenerator> cxx_module_generator = MakeRefCountedPtr<CXXModuleGenerator>();
        RC<CSharpModuleGenerator> csharp_module_generator = MakeRefCountedPtr<CSharpModuleGenerator>();

        for (const UniquePtr<Module> &mod : m_analyzer.GetModules()) {
            if (mod->GetHypClasses().Empty()) {
                continue;
            }

            batch->AddTask([this, cxx_module_generator, csharp_module_generator, mod = mod.Get()]()
            {
                HYP_LOG(BuildTool, Info, "Generating output files for module: {}", mod->GetPath());

                if (Result<void> res = cxx_module_generator->Generate(m_analyzer, *mod); res.HasError()) {
                    m_analyzer.AddError(AnalyzerError(res.GetError(), mod->GetPath()));
                }

                if (Result<void> res = csharp_module_generator->Generate(m_analyzer, *mod); res.HasError()) {
                    m_analyzer.AddError(AnalyzerError(res.GetError(), mod->GetPath()));
                }
            });
        }

        TaskSystem::GetInstance().EnqueueBatch(batch);

        return task;
    }

    void WaitWhileTaskRunning(const Task<void> &task)
    {
        Threads::AssertOnThread(ThreadName::THREAD_MAIN);

        AssertThrow(task.IsValid());

        while (!task.IsCompleted()) {
            std::putchar('.');
            
            Threads::Sleep(100);
        }

        // Newline
        std::puts("");
    }

    WorkerThreadPool    m_thread_pool;
    UniquePtr<IDriver>  m_driver;
    Analyzer            m_analyzer;
};

} // namespace buildtool
} // namespace hyperion

using namespace hyperion;
using namespace buildtool;

int main(int argc, char **argv)
{
    CommandLineParser arg_parse {
        CommandLineArgumentDefinitions()
            .Add("WorkingDirectory", "", "", CommandLineArgumentFlags::REQUIRED, CommandLineArgumentType::STRING)
            .Add("SourceDirectory", "", "", CommandLineArgumentFlags::REQUIRED, CommandLineArgumentType::STRING)
            .Add("CXXOutputDirectory", "", "", CommandLineArgumentFlags::REQUIRED, CommandLineArgumentType::STRING)
            .Add("CSharpOutputDirectory", "", "", CommandLineArgumentFlags::REQUIRED, CommandLineArgumentType::STRING)
            .Add("ExcludeDirectories", "e", "", CommandLineArgumentFlags::NONE, CommandLineArgumentType::STRING, json::JSONArray())
            .Add("Mode", "m", "", CommandLineArgumentFlags::NONE, Array<String> { "ParseHeaders" }, String("ParseHeaders"))
    };

    if (auto parse_result = arg_parse.Parse(argc, argv)) {
        TaskSystem::GetInstance().Start();

        HashSet<FilePath> exclude_directories;

        if (parse_result.GetValue().Contains("ExcludeDirectories")) {
            const json::JSONArray &exclude_directories_json = parse_result.GetValue()["ExcludeDirectories"].AsArray();

            for (const json::JSONValue &value : exclude_directories_json) {
                exclude_directories.Insert(value.ToString());
            }
        }

        buildtool::HypBuildTool build_tool {
            MakeUnique<ClangDriver>(),
            FilePath(parse_result.GetValue()["WorkingDirectory"].AsString()),
            FilePath(parse_result.GetValue()["SourceDirectory"].AsString()),
            FilePath(parse_result.GetValue()["CXXOutputDirectory"].AsString()),
            FilePath(parse_result.GetValue()["CSharpOutputDirectory"].AsString()),
            exclude_directories
        };
        
        Result<void> res = build_tool.Run();

        TaskSystem::GetInstance().Stop();

        if (res.HasError()) {
            return 1;
        }
    } else {
        HYP_LOG(BuildTool, Error, "Failed to parse arguments!\n\t{}", parse_result.GetError().GetMessage());

        return 1;
    }

    return 0;
}
