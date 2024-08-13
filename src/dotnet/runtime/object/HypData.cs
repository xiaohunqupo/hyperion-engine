using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    [StructLayout(LayoutKind.Explicit, Size = 8)]
    internal unsafe struct InternalHypDataValue
    {
        [FieldOffset(0)]
        [MarshalAs(UnmanagedType.I1)]
        public sbyte valueI8;

        [FieldOffset(0)]
        [MarshalAs(UnmanagedType.I2)]
        public short valueI16;

        [FieldOffset(0)]
        [MarshalAs(UnmanagedType.I4)]
        public int valueI32;

        [FieldOffset(0)]
        [MarshalAs(UnmanagedType.I8)]
        public long valueI64;

        [FieldOffset(0)]
        [MarshalAs(UnmanagedType.U1)]
        public byte valueU8;

        [FieldOffset(0)]
        [MarshalAs(UnmanagedType.U2)]
        public ushort valueU16;
        
        [FieldOffset(0)]
        [MarshalAs(UnmanagedType.U4)]
        public uint valueU32;

        [FieldOffset(0)]
        [MarshalAs(UnmanagedType.U8)]
        public ulong valueU64;

        [FieldOffset(0)]
        [MarshalAs(UnmanagedType.R4)]
        public float valueFloat;

        [FieldOffset(0)]
        [MarshalAs(UnmanagedType.R8)]
        public double valueDouble;

        [FieldOffset(0)]
        [MarshalAs(UnmanagedType.I1)]
        public bool valueBool;

        [FieldOffset(0)]
        [MarshalAs(UnmanagedType.U4)]
        public uint valueId;
        
        [FieldOffset(0)]
        public IntPtr valueHypObjectPtr;
    }

    /// <summary>
    ///  Represents HypData.hpp from the core/object library
    ///  Needs to be a struct to be passed by value, has a fixed size of 32 bytes
    ///  Destructor needs to be called manually (HypData_Destruct)
    /// </summary>
    [StructLayout(LayoutKind.Sequential, Size = 32)]
    internal unsafe struct HypDataBuffer
    {
        private fixed byte buffer[32];

        public TypeID TypeID
        {
            get
            {
                TypeID typeId;
                HypData_GetTypeID(ref this, out typeId);
                return typeId;
            }
        }

        public bool IsValid
        {
            get
            {
                return HypData_IsValid(ref this);
            }
        }

        public void SetValue(object? value)
        {
            if (value == null)
            {
                return;
            }

            if (value is sbyte)
            {
                HypData_SetInt8(ref this, (sbyte)value);
                return;
            }

            if (value is short)
            {
                HypData_SetInt16(ref this, (short)value);
                return;
            }

            if (value is int)
            {
                HypData_SetInt32(ref this, (int)value);
                return;
            }

            if (value is long)
            {
                HypData_SetInt64(ref this, (long)value);
                return;
            }

            if (value is byte)
            {
                HypData_SetUInt8(ref this, (byte)value);
                return;
            }

            if (value is ushort)
            {
                HypData_SetUInt16(ref this, (ushort)value);
                return;
            }

            if (value is uint)
            {
                HypData_SetUInt32(ref this, (uint)value);
                return;
            }

            if (value is ulong)
            {
                HypData_SetUInt64(ref this, (ulong)value);
                return;
            }

            if (value is float)
            {
                HypData_SetFloat(ref this, (float)value);
                return;
            }

            if (value is double)
            {
                HypData_SetDouble(ref this, (double)value);
                return;
            }

            if (value is bool)
            {
                HypData_SetBool(ref this, (bool)value);
                return;
            }

            if (value is IDBase)
            {
                HypData_SetID(ref this, ((IDBase)value).Value);
                return;
            }

            if (value is HypObject)
            {
                HypObject obj = (HypObject)value;

                HypData_SetHypObject(ref this, obj.HypClass.Address, obj.NativeAddress);
                return;
            }

            throw new NotImplementedException("Unsupported type to construct HypData: " + value.GetType());
        }

        public unsafe object? GetValue()
        {
            InternalHypDataValue value;

            if (HypData_GetInt8(ref this, out value.valueI8))
            {
                return value.valueI8;
            }

            if (HypData_GetInt16(ref this, out value.valueI16))
            {
                return value.valueI16;
            }

            if (HypData_GetInt32(ref this, out value.valueI32))
            {
                return value.valueI32;
            }

            if (HypData_GetInt64(ref this, out value.valueI64))
            {
                return value.valueI64;
            }

            if (HypData_GetUInt8(ref this, out value.valueU8))
            {
                return value.valueU8;
            }

            if (HypData_GetUInt16(ref this, out value.valueU16))
            {
                return value.valueU16;
            }

            if (HypData_GetUInt32(ref this, out value.valueU32))
            {
                return value.valueU32;
            }

            if (HypData_GetUInt64(ref this, out value.valueU64))
            {
                return value.valueU64;
            }

            if (HypData_GetFloat(ref this, out value.valueFloat))
            {
                return value.valueFloat;
            }

            if (HypData_GetDouble(ref this, out value.valueDouble))
            {
                return value.valueDouble;
            }

            if (HypData_GetBool(ref this, out value.valueBool))
            {
                return value.valueBool;
            }

            if (HypData_GetID(ref this, out value.valueId))
            {
                return new IDBase(value.valueId);
            }

            if (HypData_GetHypObject(ref this, out value.valueHypObjectPtr))
            {
                if (value.valueHypObjectPtr == IntPtr.Zero)
                {
                    return null;
                }

                return *((object*)value.valueHypObjectPtr.ToPointer());
            }

            throw new NotImplementedException("Unsupported type to get value from HypData. Current TypeID: " + TypeID.Value);
        }

        [DllImport("hyperion", EntryPoint = "HypData_Construct")]
        internal static extern void HypData_Construct([In] ref HypDataBuffer hypData);

        [DllImport("hyperion", EntryPoint = "HypData_Destruct")]
        internal static extern void HypData_Destruct([In] ref HypDataBuffer hypData);

        [DllImport("hyperion", EntryPoint = "HypData_GetTypeID")]
        internal static extern void HypData_GetTypeID([In] ref HypDataBuffer hypData, [Out] out TypeID typeId);

        [DllImport("hyperion", EntryPoint = "HypData_IsValid")]
        [return: MarshalAs(UnmanagedType.I1)]
        internal static extern bool HypData_IsValid([In] ref HypDataBuffer hypData);

        [DllImport("hyperion", EntryPoint = "HypData_GetInt8")]
        [return: MarshalAs(UnmanagedType.I1)]
        internal static extern bool HypData_GetInt8([In] ref HypDataBuffer hypData, [Out] out sbyte outValue);

        [DllImport("hyperion", EntryPoint = "HypData_GetInt16")]
        [return: MarshalAs(UnmanagedType.I1)]
        internal static extern bool HypData_GetInt16([In] ref HypDataBuffer hypData, [Out] out short outValue);

        [DllImport("hyperion", EntryPoint = "HypData_GetInt32")]
        [return: MarshalAs(UnmanagedType.I1)]
        internal static extern bool HypData_GetInt32([In] ref HypDataBuffer hypData, [Out] out int outValue);

        [DllImport("hyperion", EntryPoint = "HypData_GetInt64")]
        [return: MarshalAs(UnmanagedType.I1)]
        internal static extern bool HypData_GetInt64([In] ref HypDataBuffer hypData, [Out] out long outValue);

        [DllImport("hyperion", EntryPoint = "HypData_GetUInt8")]
        [return: MarshalAs(UnmanagedType.I1)]
        internal static extern bool HypData_GetUInt8([In] ref HypDataBuffer hypData, [Out] out byte outValue);

        [DllImport("hyperion", EntryPoint = "HypData_GetUInt16")]
        [return: MarshalAs(UnmanagedType.I1)]
        internal static extern bool HypData_GetUInt16([In] ref HypDataBuffer hypData, [Out] out ushort outValue);

        [DllImport("hyperion", EntryPoint = "HypData_GetUInt32")]
        [return: MarshalAs(UnmanagedType.I1)]
        internal static extern bool HypData_GetUInt32([In] ref HypDataBuffer hypData, [Out] out uint outValue);

        [DllImport("hyperion", EntryPoint = "HypData_GetUInt64")]
        [return: MarshalAs(UnmanagedType.I1)]
        internal static extern bool HypData_GetUInt64([In] ref HypDataBuffer hypData, [Out] out ulong outValue);

        [DllImport("hyperion", EntryPoint = "HypData_GetFloat")]
        [return: MarshalAs(UnmanagedType.I1)]
        internal static extern bool HypData_GetFloat([In] ref HypDataBuffer hypData, [Out] out float outValue);

        [DllImport("hyperion", EntryPoint = "HypData_GetDouble")]
        [return: MarshalAs(UnmanagedType.I1)]
        internal static extern bool HypData_GetDouble([In] ref HypDataBuffer hypData, [Out] out double outValue);

        [DllImport("hyperion", EntryPoint = "HypData_GetBool")]
        [return: MarshalAs(UnmanagedType.I1)]
        internal static extern bool HypData_GetBool([In] ref HypDataBuffer hypData, [Out] out bool outValue);

        [DllImport("hyperion", EntryPoint = "HypData_GetHypObject")]
        [return: MarshalAs(UnmanagedType.I1)]
        internal static extern bool HypData_GetHypObject([In] ref HypDataBuffer hypData, [Out] out IntPtr outObjectPtr);

        [DllImport("hyperion", EntryPoint = "HypData_GetID")]
        [return: MarshalAs(UnmanagedType.I1)]
        internal static extern bool HypData_GetID([In] ref HypDataBuffer hypData, [Out] out uint outIdValue);

        [DllImport("hyperion", EntryPoint = "HypData_IsInt8")]
        [return: MarshalAs(UnmanagedType.I1)]
        internal static extern bool HypData_IsInt8([In] ref HypDataBuffer hypData);

        [DllImport("hyperion", EntryPoint = "HypData_IsInt16")]
        [return: MarshalAs(UnmanagedType.I1)]
        internal static extern bool HypData_IsInt16([In] ref HypDataBuffer hypData);

        [DllImport("hyperion", EntryPoint = "HypData_IsInt32")]
        [return: MarshalAs(UnmanagedType.I1)]
        internal static extern bool HypData_IsInt32([In] ref HypDataBuffer hypData);

        [DllImport("hyperion", EntryPoint = "HypData_IsInt64")]
        [return: MarshalAs(UnmanagedType.I1)]
        internal static extern bool HypData_IsInt64([In] ref HypDataBuffer hypData);

        [DllImport("hyperion", EntryPoint = "HypData_IsUInt8")]
        [return: MarshalAs(UnmanagedType.I1)]
        internal static extern bool HypData_IsUInt8([In] ref HypDataBuffer hypData);

        [DllImport("hyperion", EntryPoint = "HypData_IsUInt16")]
        [return: MarshalAs(UnmanagedType.I1)]
        internal static extern bool HypData_IsUInt16([In] ref HypDataBuffer hypData);

        [DllImport("hyperion", EntryPoint = "HypData_IsUInt32")]
        [return: MarshalAs(UnmanagedType.I1)]
        internal static extern bool HypData_IsUInt32([In] ref HypDataBuffer hypData);

        [DllImport("hyperion", EntryPoint = "HypData_IsUInt64")]
        [return: MarshalAs(UnmanagedType.I1)]
        internal static extern bool HypData_IsUInt64([In] ref HypDataBuffer hypData);

        [DllImport("hyperion", EntryPoint = "HypData_IsFloat")]
        [return: MarshalAs(UnmanagedType.I1)]
        internal static extern bool HypData_IsFloat([In] ref HypDataBuffer hypData);

        [DllImport("hyperion", EntryPoint = "HypData_IsDouble")]
        [return: MarshalAs(UnmanagedType.I1)]
        internal static extern bool HypData_IsDouble([In] ref HypDataBuffer hypData);

        [DllImport("hyperion", EntryPoint = "HypData_IsBool")]
        [return: MarshalAs(UnmanagedType.I1)]
        internal static extern bool HypData_IsBool([In] ref HypDataBuffer hypData);

        [DllImport("hyperion", EntryPoint = "HypData_IsHypObject")]
        [return: MarshalAs(UnmanagedType.I1)]
        internal static extern bool HypData_IsHypObject([In] ref HypDataBuffer hypData);

        [DllImport("hyperion", EntryPoint = "HypData_IsID")]
        [return: MarshalAs(UnmanagedType.I1)]
        internal static extern bool HypData_IsID([In] ref HypDataBuffer hypData);

        [DllImport("hyperion", EntryPoint = "HypData_SetInt8")]
        [return: MarshalAs(UnmanagedType.I1)]
        internal static extern bool HypData_SetInt8([In] ref HypDataBuffer hypData, sbyte value);

        [DllImport("hyperion", EntryPoint = "HypData_SetInt16")]
        [return: MarshalAs(UnmanagedType.I1)]
        internal static extern bool HypData_SetInt16([In] ref HypDataBuffer hypData, short value);

        [DllImport("hyperion", EntryPoint = "HypData_SetInt32")]
        [return: MarshalAs(UnmanagedType.I1)]
        internal static extern bool HypData_SetInt32([In] ref HypDataBuffer hypData, int value);

        [DllImport("hyperion", EntryPoint = "HypData_SetInt64")]
        [return: MarshalAs(UnmanagedType.I1)]
        internal static extern bool HypData_SetInt64([In] ref HypDataBuffer hypData, long value);

        [DllImport("hyperion", EntryPoint = "HypData_SetUInt8")]
        [return: MarshalAs(UnmanagedType.I1)]
        internal static extern bool HypData_SetUInt8([In] ref HypDataBuffer hypData, byte value);

        [DllImport("hyperion", EntryPoint = "HypData_SetUInt16")]
        [return: MarshalAs(UnmanagedType.I1)]
        internal static extern bool HypData_SetUInt16([In] ref HypDataBuffer hypData, ushort value);

        [DllImport("hyperion", EntryPoint = "HypData_SetUInt32")]
        [return: MarshalAs(UnmanagedType.I1)]
        internal static extern bool HypData_SetUInt32([In] ref HypDataBuffer hypData, uint value);

        [DllImport("hyperion", EntryPoint = "HypData_SetUInt64")]
        [return: MarshalAs(UnmanagedType.I1)]
        internal static extern bool HypData_SetUInt64([In] ref HypDataBuffer hypData, ulong value);

        [DllImport("hyperion", EntryPoint = "HypData_SetFloat")]
        [return: MarshalAs(UnmanagedType.I1)]
        internal static extern bool HypData_SetFloat([In] ref HypDataBuffer hypData, float value);

        [DllImport("hyperion", EntryPoint = "HypData_SetDouble")]
        [return: MarshalAs(UnmanagedType.I1)]
        internal static extern bool HypData_SetDouble([In] ref HypDataBuffer hypData, double value);

        [DllImport("hyperion", EntryPoint = "HypData_SetBool")]
        [return: MarshalAs(UnmanagedType.I1)]
        internal static extern bool HypData_SetBool([In] ref HypDataBuffer hypData, bool value);

        [DllImport("hyperion", EntryPoint = "HypData_SetID")]
        [return: MarshalAs(UnmanagedType.I1)]
        internal static extern bool HypData_SetID([In] ref HypDataBuffer hypData, uint id);

        [DllImport("hyperion", EntryPoint = "HypData_SetHypObject")]
        [return: MarshalAs(UnmanagedType.I1)]
        internal static extern bool HypData_SetHypObject([In] ref HypDataBuffer hypData, [In] IntPtr hypClassPtr, [In] IntPtr nativeAddress);
    }

    public class HypData
    {
        private HypDataBuffer _data;

        public HypData(object? value)
        {
            HypDataBuffer.HypData_Construct(ref _data);

            if (value != null)
            {
                _data.SetValue(value);
            }
        }

        internal HypData(HypDataBuffer data)
        {
            _data = data;
        }

        ~HypData()
        {
            HypDataBuffer.HypData_Destruct(ref _data);
        }

        public TypeID TypeID
        {
            get
            {
                return _data.TypeID;
            }
        }

        public bool IsValid
        {
            get
            {
                return _data.IsValid;
            }
        }

        internal HypDataBuffer Buffer
        {
            get
            {
                return _data;
            }
        }

        public object? GetValue()
        {
            return _data.GetValue();
        }

        public override string ToString()
        {
            return GetValue()?.ToString() ?? "null";
        }
    }
}