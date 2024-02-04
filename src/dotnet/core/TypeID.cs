using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    /// <summary>
    ///  Represents a native (C++) TypeID (see core/lib/TypeID.hpp)
    /// </summary>
    
    [StructLayout(LayoutKind.Sequential, Size = 4)]
    public struct TypeID
    {
        private uint value;

        public TypeID(uint value)
        {
            this.value = value;
        }

        public uint Value
        {
            get
            {
                return value;
            }
        }

        /// <summary>
        /// Returns the native TypeID for the given type, if it has been registered from C++ side
        /// </summary>
        /// <typeparam name="T">The C# type to lookup the TypeID of</typeparam>
        /// <returns>TypeID</returns>
        public static TypeID ForType<T>()
        {
            TypeID value = TypeID_ForDynamicType(typeof(T).Name);

            if (value.Value == 0)
            {
                throw new InvalidOperationException($"TypeID for {typeof(T).Name} has not been registered from C++ side");
            }

            return value;
        }

        [DllImport("libhyperion", EntryPoint = "TypeID_ForDynamicType")]
        private static extern TypeID TypeID_ForDynamicType(string typeName);
    }
}