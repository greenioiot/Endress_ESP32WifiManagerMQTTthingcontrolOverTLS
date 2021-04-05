#define ID_meter  0x01
#define Total_of_Reg  1

#define Reg_RN                0x0001      //  0.
//#define Reg_SN                0x0008      //  1.
//#define Reg_TN                0x0004      //  2.


uint16_t const Reg_addr[1] = {
  Reg_RN,
//  Reg_SN,
//  Reg_TN,

};

float DATA_METER [Total_of_Reg] ;
