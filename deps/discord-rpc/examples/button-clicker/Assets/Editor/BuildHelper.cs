using UnityEditor;
using System.Diagnostics;
using System.IO;

[InitializeOnLoad]
public class ScriptBatch
{
	static ScriptBatch()
	{
		EnsureDLL();
	}

	public static bool FileExists(string filename)
	{
		return new FileInfo(filename).Exists;
	}

	public static bool RunRpcBuildScript()
	{
		UnityEngine.Debug.Log("Try to run build script");

		Process proc = new Process();
#if UNITY_STANDALONE_OSX || UNITY_EDITOR_OSX
		proc.StartInfo.UseShellExecute = false;
		// brew installs cmake in /usr/local/bin, which Unity seems to strip from PATH?
		string newPath = proc.StartInfo.EnvironmentVariables["PATH"] + ":/usr/local/bin";
		proc.StartInfo.EnvironmentVariables["PATH"] = newPath;
#endif
		proc.StartInfo.FileName = "python";
		proc.StartInfo.Arguments = "build.py unity";
		proc.StartInfo.WorkingDirectory = "../..";
		proc.Start();
		proc.WaitForExit();
		return proc.ExitCode == 0;
	}

	public static void EnsureDLL()
	{
		#if UNITY_STANDALONE_WIN || UNITY_EDITOR_WIN
		string[] dstDirs = { "Assets/Plugins", "Assets/Plugins/x86", "Assets/Plugins/x86_64" };
		string[] dstDlls = { "Assets/Plugins/x86/discord-rpc.dll", "Assets/Plugins/x86_64/discord-rpc.dll" };
		string[] srcDlls = { "../../builds/install/win64-dynamic/bin/discord-rpc.dll", "../../builds/install/win64-dynamic/bin/discord-rpc.dll" };
		#elif UNITY_STANDALONE_OSX || UNITY_EDITOR_OSX
		string[] dstDirs = { "Assets/Plugins" };
		string[] dstDlls = { "Assets/Plugins/discord-rpc.bundle" };
		string[] srcDlls = { "../../builds/install/osx-dynamic/lib/libdiscord-rpc.dylib" };
		#else
		string[] dstDirs = { "Assets/Plugins", "Assets/Plugins/x86", "Assets/Plugins/x86_64" };
		string[] dstDlls = { "Assets/Plugins/discord-rpc.so" };
		string[] srcDlls = { "../../builds/install/linux-dynamic/lib/libdiscord-rpc.so" };
		#endif

		Debug.Assert(dstDlls.Length == srcDlls.Length);

		bool exists = true;
		foreach (string fname in dstDlls)
		{
			if (!FileExists(fname))
			{
				exists = false;
				break;
			}
		}

		if (exists)
		{
			return;
		}

		exists = true;
		foreach (string fname in srcDlls)
		{
			if (!FileExists(fname))
			{
				exists = false;
				break;
			}
		}

		if (!exists)
		{
			if (!RunRpcBuildScript())
			{
				UnityEngine.Debug.LogError("Build failed");
				return;
			}
		}

		// make sure the dirs exist
		foreach (string dirname in dstDirs)
		{
			Directory.CreateDirectory(dirname);
		}

		// Copy dlls
		for (int i = 0; i < dstDlls.Length; ++i)
		{
			FileUtil.CopyFileOrDirectory(srcDlls[i], dstDlls[i]);
		}
	}
}