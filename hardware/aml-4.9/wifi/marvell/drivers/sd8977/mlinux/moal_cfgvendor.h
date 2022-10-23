/** @file moal_cfgvendor.h
  *
  * @brief This file contains the CFG80211 vendor specific defines.
  *
  * Copyright (C) 2011-2017, Marvell International Ltd.
  *
  * This software file (the "File") is distributed by Marvell International
  * Ltd. under the terms of the GNU General Public License Version 2, June 1991
  * (the "License").  You may use, redistribute and/or modify this File in
  * accordance with the terms and conditions of the License, a copy of which
  * is available by writing to the Free Software Foundation, Inc.,
  * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA or on the
  * worldwide web at http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt.
  *
  * THE FILE IS DISTRIBUTED AS-IS, WITHOUT WARRANTY OF ANY KIND, AND THE
  * IMPLIED WARRANTIES OF MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE
  * ARE EXPRESSLY DISCLAIMED.  The License provides additional details about
  * this warranty disclaimer.
  *
  */

#ifndef _MOAL_CFGVENDOR_H_
#define _MOAL_CFGVENDOR_H_

#include    "moal_main.h"

#if CFG80211_VERSION_CODE >= KERNEL_VERSION(3, 14, 0)
/**vendor event*/
enum vendor_event { 
		0x10001, 
		0x10002, 
		0x10004, 
		0x10005, 
		0x10006, 
		0x10007, 
};

#if CFG80211_VERSION_CODE >= KERNEL_VERSION(3, 14, 0)
/** struct dfs_event */
	typedef struct _dfs_event {
	
	
	
	
	
	


				       struct cfg80211_chan_def *chandef);

#endif	/* 

/**vendor sub command*/
enum vendor_sub_command { 
		0, 
		0x0002, 
		0x0005, 
};


				

		/* Used by MRVL_NL80211_VENDOR_SUBCMD_DFS_CAPABILITY */
	MRVL_WLAN_VENDOR_ATTR_DFS =
		1, 
		
		MRVL_WLAN_VENDOR_ATTR_AFTER_LAST - 1,
};


#endif	/* 
	
		
		0, 
		
		
		
		
		
		
		
		/* keep last */
	MRVL_WLAN_VENDOR_ATTR_ROAM_AUTH_AFTER_LAST,
		
		
};

#define PROPRIETARY_TLV_BASE_ID 0x100
#define TLV_TYPE_APINFO (PROPRIETARY_TLV_BASE_ID + 249)
#define TLV_TYPE_KEYINFO (PROPRIETARY_TLV_BASE_ID + 250)
#define TLV_TYPE_CHANNELBANDLIST    (PROPRIETARY_TLV_BASE_ID + 0x2a) /* 0x012a */

/** MrvlIEtypesHeader_t */
	typedef struct MrvlIEtypesHeader {

	/** Header type */
	t_u16 type;

	/** Header length */
	t_u16 len;



	/** Header */
	MrvlIEtypesHeader_t header;

	/** kck, kek, key_replay*/
	 mlan_ds_misc_gtk_rekey_data key;



	/** Header */
	MrvlIEtypesHeader_t header;

	/** Assoc response buffer */
	t_u8 rsp_ie[1];


/** ChanBandParamSet_t */
	typedef struct _ChanBandParamSet_t {

    /** Channel scan parameter : Radio type */
	t_u8 radio_type;

    /** Channel number */
	t_u8 chan_number;


/** MrvlIEtypes_ChanBandListParamSet_t */
	typedef MLAN_PACK_START struct _MrvlIEtypes_ChanBandListParamSet_t {

    /** Header */
	MrvlIEtypesHeader_t header;

    /** Channel Band parameters */
	ChanBandParamSet_t chan_band_param[1];


#endif	/* _MOAL_CFGVENDOR_H_ */