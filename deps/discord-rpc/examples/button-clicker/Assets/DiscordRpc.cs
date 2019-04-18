using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using System.Text;

public class DiscordRpc
{
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate void ReadyCallback(ref DiscordUser connectedUser);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate void DisconnectedCallback(int errorCode, string message);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate void ErrorCallback(int errorCode, string message);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate void JoinCallback(string secret);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate void SpectateCallback(string secret);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate void RequestCallback(ref DiscordUser request);

    public struct EventHandlers
    {
        public ReadyCallback readyCallback;
        public DisconnectedCallback disconnectedCallback;
        public ErrorCallback errorCallback;
        public JoinCallback joinCallback;
        public SpectateCallback spectateCallback;
        public RequestCallback requestCallback;
    }

    [Serializable, StructLayout(LayoutKind.Sequential)]
    public struct RichPresenceStruct
    {
        public IntPtr state; /* max 128 bytes */
        public IntPtr details; /* max 128 bytes */
        public long startTimestamp;
        public long endTimestamp;
        public IntPtr largeImageKey; /* max 32 bytes */
        public IntPtr largeImageText; /* max 128 bytes */
        public IntPtr smallImageKey; /* max 32 bytes */
        public IntPtr smallImageText; /* max 128 bytes */
        public IntPtr partyId; /* max 128 bytes */
        public int partySize;
        public int partyMax;
        public IntPtr matchSecret; /* max 128 bytes */
        public IntPtr joinSecret; /* max 128 bytes */
        public IntPtr spectateSecret; /* max 128 bytes */
        public bool instance;
    }

    [Serializable]
    public struct DiscordUser
    {
        public string userId;
        public string username;
        public string discriminator;
        public string avatar;
    }

    public enum Reply
    {
        No = 0,
        Yes = 1,
        Ignore = 2
    }

    [DllImport("discord-rpc", EntryPoint = "Discord_Initialize", CallingConvention = CallingConvention.Cdecl)]
    public static extern void Initialize(string applicationId, ref EventHandlers handlers, bool autoRegister, string optionalSteamId);

    [DllImport("discord-rpc", EntryPoint = "Discord_Shutdown", CallingConvention = CallingConvention.Cdecl)]
    public static extern void Shutdown();

    [DllImport("discord-rpc", EntryPoint = "Discord_RunCallbacks", CallingConvention = CallingConvention.Cdecl)]
    public static extern void RunCallbacks();

    [DllImport("discord-rpc", EntryPoint = "Discord_UpdatePresence", CallingConvention = CallingConvention.Cdecl)]
    private static extern void UpdatePresenceNative(ref RichPresenceStruct presence);

    [DllImport("discord-rpc", EntryPoint = "Discord_ClearPresence", CallingConvention = CallingConvention.Cdecl)]
    public static extern void ClearPresence();

    [DllImport("discord-rpc", EntryPoint = "Discord_Respond", CallingConvention = CallingConvention.Cdecl)]
    public static extern void Respond(string userId, Reply reply);

    [DllImport("discord-rpc", EntryPoint = "Discord_UpdateHandlers", CallingConvention = CallingConvention.Cdecl)]
    public static extern void UpdateHandlers(ref EventHandlers handlers);

    public static void UpdatePresence(RichPresence presence)
    {
        var presencestruct = presence.GetStruct();
        UpdatePresenceNative(ref presencestruct);
        presence.FreeMem();
    }

    public class RichPresence
    {
        private RichPresenceStruct _presence;
        private readonly List<IntPtr> _buffers = new List<IntPtr>(10);

        public string state; /* max 128 bytes */
        public string details; /* max 128 bytes */
        public long startTimestamp;
        public long endTimestamp;
        public string largeImageKey; /* max 32 bytes */
        public string largeImageText; /* max 128 bytes */
        public string smallImageKey; /* max 32 bytes */
        public string smallImageText; /* max 128 bytes */
        public string partyId; /* max 128 bytes */
        public int partySize;
        public int partyMax;
        public string matchSecret; /* max 128 bytes */
        public string joinSecret; /* max 128 bytes */
        public string spectateSecret; /* max 128 bytes */
        public bool instance;

        /// <summary>
        /// Get the <see cref="RichPresenceStruct"/> reprensentation of this instance
        /// </summary>
        /// <returns><see cref="RichPresenceStruct"/> reprensentation of this instance</returns>
        internal RichPresenceStruct GetStruct()
        {
            if (_buffers.Count > 0)
            {
                FreeMem();
            }

            _presence.state = StrToPtr(state);
            _presence.details = StrToPtr(details);
            _presence.startTimestamp = startTimestamp;
            _presence.endTimestamp = endTimestamp;
            _presence.largeImageKey = StrToPtr(largeImageKey);
            _presence.largeImageText = StrToPtr(largeImageText);
            _presence.smallImageKey = StrToPtr(smallImageKey);
            _presence.smallImageText = StrToPtr(smallImageText);
            _presence.partyId = StrToPtr(partyId);
            _presence.partySize = partySize;
            _presence.partyMax = partyMax;
            _presence.matchSecret = StrToPtr(matchSecret);
            _presence.joinSecret = StrToPtr(joinSecret);
            _presence.spectateSecret = StrToPtr(spectateSecret);
            _presence.instance = instance;

            return _presence;
        }

        /// <summary>
        /// Returns a pointer to a representation of the given string with a size of maxbytes
        /// </summary>
        /// <param name="input">String to convert</param>
        /// <returns>Pointer to the UTF-8 representation of <see cref="input"/></returns>
        private IntPtr StrToPtr(string input)
        {
            if (string.IsNullOrEmpty(input)) return IntPtr.Zero;
            var convbytecnt = Encoding.UTF8.GetByteCount(input);
            var buffer = Marshal.AllocHGlobal(convbytecnt + 1);
            for (int i = 0; i < convbytecnt + 1; i++)
            {
                Marshal.WriteByte(buffer, i , 0);
            }
            _buffers.Add(buffer);
            Marshal.Copy(Encoding.UTF8.GetBytes(input), 0, buffer, convbytecnt);
            return buffer;
        }

        /// <summary>
        /// Convert string to UTF-8 and add null termination
        /// </summary>
        /// <param name="toconv">string to convert</param>
        /// <returns>UTF-8 representation of <see cref="toconv"/> with added null termination</returns>
        private static string StrToUtf8NullTerm(string toconv)
        {
            var str = toconv.Trim();
            var bytes = Encoding.Default.GetBytes(str);
            if (bytes.Length > 0 && bytes[bytes.Length - 1] != 0)
            {
                str += "\0\0";
            }
            return Encoding.UTF8.GetString(Encoding.UTF8.GetBytes(str));
        }

        /// <summary>
        /// Free the allocated memory for conversion to <see cref="RichPresenceStruct"/>
        /// </summary>
        internal void FreeMem()
        {
            for (var i = _buffers.Count - 1; i >= 0; i--)
            {
                Marshal.FreeHGlobal(_buffers[i]);
                _buffers.RemoveAt(i);
            }
        }
    }
}