//==============================================================================
//
// Title:		atom_16ch_dsa_api.h
// Purpose:		A short description of the interface.
//
// Created on:	2019-07-08 at 14:24:53 by localadmin.
// Copyright:	. All Rights Reserved.
//
//==============================================================================

#ifndef __atom_16ch_dsa_api_H__
#define __atom_16ch_dsa_api_H__

#ifdef __cplusplus
    extern "C" {
#endif


//gai dong
//#ifdef _DEBUG
//#pragma comment(lib,"C:\\Users\\22851\\Desktop\\XSAMPLE\\XBackradar3.0\\atom_16ch_dsa_api.lib")
//#else
//#pragma comment(lib,"C:\\Users\\22851\\Desktop\\XSAMPLE\\XBackradar3.0\\atom_16ch_dsa_api.lib")
//#endif
//==============================================================================
// Include files

//==============================================================================
// Constants

//==============================================================================
// Types

//==============================================================================
// External variables

//==============================================================================
// Global functions

int dsa_16ch_open();
void dsa_16ch_close();
int dsa_16ch_sample_rate_sel(unsigned int sample_rate_sel);
int dsa_16ch_sample_mode_set(unsigned int sample_mode);
int dsa_16ch_trig_src_sel(unsigned int trig_src_sel);
int dsa_16ch_ext_trig_edge_sel(unsigned int ext_trig_edge_sel);
int dsa_16ch_clock_base_sel(unsigned int clock_base_sel);
int dsa_16ch_fix_point_mode_point_num_per_ch_set(unsigned int point_num_per_ch);
int dsa_16ch_start();
int dsa_16ch_stop();
int dsa_16ch_point_num_per_ch_in_buf_query(unsigned int *p_point_num_per_ch);
int dsa_16ch_read_data(unsigned int point_num2read, double *p_ch0,double *p_ch1,double *p_ch2,double *p_ch3,double *p_ch4,double *p_ch5,double *p_ch6,double *p_ch7,
					 							  double *p_ch8,double *p_ch9,double *p_ch10,double *p_ch11,double *p_ch12,double *p_ch13,double *p_ch14,double *p_ch15);
int dsa_16ch_buf_overflow_query(unsigned int *p_overflow);
int dsa_16ch_dio_dir_set(unsigned int group_index,unsigned int dio_dir);
int dsa_16ch_wr_do_data(unsigned int  group_index,unsigned char do_data);
int dsa_16ch_rd_di_data(unsigned int  group_index,unsigned char *p_di_data);

#ifdef __cplusplus
    }
#endif

#endif  /* ndef __atom_16ch_dsa_api_H__ */
