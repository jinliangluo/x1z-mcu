/*
 * can_drv.h
 *
 * Created: 2018/10/3 18:17:51
 *  Author: Administrator
 */ 


#ifndef CAN_DRV_H_
#define CAN_DRV_H_

#ifdef __cplusplus
extern "C" {
#endif

#define CAN_EFF_FLAG 0x80000000U    //��չ֡�ı�ʶ
#define CAN_RTR_FLAG 0x40000000U    //Զ��֡�ı�ʶ
#define CAN_ERR_FLAG 0x20000000U    //����֡�ı�ʶ�����ڴ�����



void can_drv_init(void);

#ifdef __cplusplus
}
#endif

#endif /* CAN_DRV_H_ */