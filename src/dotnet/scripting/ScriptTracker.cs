using System;
using System.IO;
using System.Runtime.InteropServices;

namespace Hyperion
{
    public delegate void ScriptEventCallback(IntPtr selfPtr, ScriptEvent scriptEvent);

    public class ScriptTracker
    {
        private FileSystemWatcher watcher;
        private ScriptEventCallback callback;
        private IntPtr callbackSelfPtr;

        private ScriptCompiler? scriptCompiler = null;

        private Dictionary<string, ManagedScriptWrapper> processingScripts = new Dictionary<string, ManagedScriptWrapper>();

        public void Initialize(string sourceDirectory, string intermediateDirectory, string binaryOutputDirectory, IntPtr callbackPtr, IntPtr callbackSelfPtr)
        {
            this.callback = Marshal.GetDelegateForFunctionPointer<ScriptEventCallback>(callbackPtr);
            this.callbackSelfPtr = callbackSelfPtr;

            scriptCompiler = new ScriptCompiler(sourceDirectory, intermediateDirectory, binaryOutputDirectory);
            scriptCompiler.BuildAllProjects();

            watcher = new FileSystemWatcher(sourceDirectory);
            watcher.NotifyFilter = NotifyFilters.LastWrite;
            watcher.Filter = "*.cs";
            watcher.Changed += OnFileChanged;
            watcher.EnableRaisingEvents = true;
            watcher.IncludeSubdirectories = true;
        }

        public void Update()
        {
            Console.WriteLine("ScriptTracker.Update() called from C#. Currently have {0} scripts processing", processingScripts.Count);

            List<string> scriptsToRemove = new List<string>();

            foreach (KeyValuePair<string, ManagedScriptWrapper> entry in processingScripts)
            {
                if (!entry.Value.IsValid)
                {
                    scriptsToRemove.Add(entry.Key);

                    continue;
                }

                Console.WriteLine("ScriptTracker.Update() : {0} is processing", entry.Key);

                if (entry.Value.Get().State != ManagedScriptState.Processing)
                {
                    TriggerCallback(new ScriptEvent
                    {
                        Type = ScriptEventType.StateChanged,
                        ScriptPtr = entry.Value.Address
                    });

                    scriptsToRemove.Add(entry.Key);

                    continue;
                }

                // script processing - compile using Roslyn
                // just testing for now
                if (scriptCompiler != null)
                {
                    scriptCompiler.Compile(entry.Key);

                    entry.Value.Get().State = ManagedScriptState.Compiled;
                }
                else
                {
                    Console.WriteLine("ScriptTracker.Update() : scriptCompiler is null");

                    entry.Value.Get().State = ManagedScriptState.Errored;
                }
            }

            foreach (string scriptPath in scriptsToRemove)
            {
                processingScripts.Remove(scriptPath);
            }
        }

        private void OnFileChanged(object source, FileSystemEventArgs e)
        {
            Console.WriteLine("ScriptTracker.OnFileChanged() : {0}", e.FullPath);

            if (processingScripts.ContainsKey(e.FullPath))
            {
                Console.WriteLine("ScriptTracker.OnFileChanged() : {0} is already being processed", e.FullPath);

                return;
            }

            ManagedScriptWrapper managedScriptWrapper = new ManagedScriptWrapper(new ManagedScript
            {
                Path = e.FullPath,
                State = ManagedScriptState.Processing
            });

            processingScripts.Add(e.FullPath, managedScriptWrapper);

            TriggerCallback(new ScriptEvent
            {
                Type = ScriptEventType.StateChanged,
                ScriptPtr = managedScriptWrapper.Address
            });
        }

        private void TriggerCallback(ScriptEvent scriptEvent)
        {
            callback(callbackSelfPtr, scriptEvent);
        }
    }
}