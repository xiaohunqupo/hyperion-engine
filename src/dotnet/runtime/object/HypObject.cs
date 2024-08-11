using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    public class HypObject
    {
        public IntPtr _hypClassPtr;
        public IntPtr _nativeAddress;

        // public void InitializeHypObject(IntPtr hypClassPtr, IntPtr nativeAddress)
        // {
        //     this.hypClass = new HypClass(hypClassPtr);
        //     this.nativeAddress = nativeAddress;
        // }

        protected HypObject()
        {
            Console.WriteLine("HypObject constructor, _hypClassPtr: " + _hypClassPtr + ", _nativeAddress: " + _nativeAddress);

            if (_hypClassPtr == IntPtr.Zero)
            {
                if (_nativeAddress != IntPtr.Zero)
                {
                    throw new Exception("Native address is not null - object is already initialized");
                }

                // Read the HypClassBinding attribute
                HypClassBinding attribute = (HypClassBinding)Attribute.GetCustomAttribute(this.GetType(), typeof(HypClassBinding));

                if (attribute == null)
                {
                    throw new Exception("Failed to get HypClassBinding attribute");
                }

                HypClass hypClass = attribute.LoadHypClass();

                if (hypClass == HypClass.Invalid)
                {
                    throw new Exception("Invalid HypClass returned from HypClassBinding attribute");
                }

                _hypClassPtr = hypClass.Address;
                _nativeAddress = hypClass.CreateInstance();
            }

            if (_hypClassPtr == IntPtr.Zero)
            {
                throw new Exception("HypClass pointer is null - object is not correctly initialized");
            }

            if (_nativeAddress == IntPtr.Zero)
            {
                throw new Exception("Native address is null - object is not correctly initialized");
            }

            HypObject_Verify(_hypClassPtr, _nativeAddress);
        }

        ~HypObject()
        {
            if (IsValid)
            {
                HypObject_DecRef(_hypClassPtr, _nativeAddress);
            }
        }

        public bool IsValid
        {
            get
            {
                return _hypClassPtr != IntPtr.Zero
                    && _nativeAddress != IntPtr.Zero;
            }
        }

        public HypClass HypClass
        {
            get
            {
                return new HypClass(_hypClassPtr);
            }
        }

        public IntPtr NativeAddress
        {
            get
            {
                return _nativeAddress;
            }
        }

        protected HypProperty GetProperty(Name name)
        {
            if (_hypClassPtr == IntPtr.Zero)
            {
                throw new Exception("HypClass pointer is null");
            }

            IntPtr propertyPtr = HypObject_GetProperty(_hypClassPtr, ref name);

            if (propertyPtr == IntPtr.Zero)
            {
                string propertiesString = "";

                foreach (HypProperty property in HypClass.Properties)
                {
                    propertiesString += property.Name + ", ";
                }

                throw new Exception("Failed to get property \"" + name + "\" from HypClass \"" + HypClass.Name + "\". Available properties: " + propertiesString);
            }

            return new HypProperty(propertyPtr);
        }

        [DllImport("hyperion", EntryPoint = "HypObject_Verify")]
        private static extern void HypObject_Verify([In] IntPtr hypClassPtr, [In] IntPtr nativeAddress);

        [DllImport("hyperion", EntryPoint = "HypObject_IncRef")]
        private static extern void HypObject_IncRef([In] IntPtr hypClassPtr, [In] IntPtr nativeAddress);

        [DllImport("hyperion", EntryPoint = "HypObject_DecRef")]
        private static extern void HypObject_DecRef([In] IntPtr hypClassPtr, [In] IntPtr nativeAddress);

        [DllImport("hyperion", EntryPoint = "HypObject_GetProperty")]
        private static extern IntPtr HypObject_GetProperty([In] IntPtr hypClassPtr, [In] ref Name name);
    }
}