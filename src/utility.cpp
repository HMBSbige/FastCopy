﻿static char *utility_id = 
	"@(#)Copyright (C) 2004-2018 H.Shirouzu		utility.cpp	ver3.50";
/* ========================================================================
	Project  Name			: general routine
	Create					: 2004-09-15(Wed)
	Update					: 2018-05-28(Mon)
	Copyright				: H.Shirouzu
	License					: GNU General Public License version 3
	======================================================================== */

#include <stdio.h>
#include <stddef.h>
#include "utility.h"
#include <aclapi.h>

using namespace std;

/*=========================================================================
	PathArray::PathObj
=========================================================================*/
PathArray::PathObj& PathArray::PathObj::operator=(const PathArray::PathObj& init)
{
	if (&init == this) {
		return	*this;
	}
	path	= make_unique<WCHAR[]>(init.len + 1);
	memcpy(path.get(), init.path.get(), (init.len + 1) * sizeof(WCHAR));

	uppr	= make_unique<WCHAR[]>(init.upprLen + 1);
	memcpy(uppr.get(), init.uppr.get(), (init.upprLen + 1) * sizeof(WCHAR));

	len		= init.len;
	upprLen	= init.upprLen;
	isDir	= init.isDir;
	isDirStrip = init.isDirStrip;
	hashId	= init.hashId;

	return	*this;
}

BOOL PathArray::PathObj::Set(const WCHAR *_path)
{
	path = NULL;
	uppr = NULL;
	len = 0;
	isDir = FALSE;

	if (_path && (len = (int)wcslen(_path)) > 0) {
		int	alloc_len = len + 1;

		path = make_unique<WCHAR[]>(alloc_len);
		memcpy(path.get(), _path, alloc_len * sizeof(WCHAR));

		upprLen = len;
		auto ch = _path[len-1];
		if (ch == '\\' || ch == '/') {
			isDir = TRUE;
			if (isDirStrip) {
				upprLen--;
			}
		}
		uppr = make_unique<WCHAR[]>(upprLen + 1);
		memcpy(uppr.get(), _path, upprLen * sizeof(WCHAR));
		uppr[upprLen] = 0;

		::CharUpperW(uppr.get());
		hashId = MakeHash(uppr.get(), upprLen * sizeof(WCHAR));
	}
	return	TRUE;
}

int PathArray::PathObj::Get(WCHAR *_path)
{
	if (!path) return 0;

	memcpy(_path, path.get(), (len + 1) * sizeof(WCHAR));

	return	len;
}

/*=========================================================================
	PathArray
=========================================================================*/
PathArray::PathArray(DWORD _flags) : THashTbl(10000)
{
	num = 0;
	pathArray = NULL;
	flags = _flags;
}

PathArray::PathArray(const PathArray &src) : THashTbl(10000)
{
	num = 0;
	pathArray = NULL;
	*this = src;
	flags = 0;
}

PathArray::~PathArray()
{
	Init();
}

void PathArray::Init(void)
{
	while (--num >= 0) {
		delete pathArray[num];
	}
	free(pathArray);
	num = 0;
	pathArray = NULL;
}

BOOL PathArray::IsSameVal(THashObj *_obj, const void *_val)
{
	PathObj	*p1 = (PathObj *)_obj;
	PathObj	*p2 = (PathObj *)_val;

	return	p1->hashId == p2->hashId &&
			wcscmp(p1->uppr.get(), p2->uppr.get()) == 0;
}


int PathArray::RegisterMultiPath(const WCHAR *_multi_path, const WCHAR *separator)
{
	WCHAR	*multi_path = wcsdup(_multi_path);
	WCHAR	*tok, *p;
	int		cnt = 0;
	BOOL	is_remove_quote = (flags & NO_REMOVE_QUOTE) == 0 ? TRUE : FALSE;

	for (tok = strtok_pathW(multi_path, separator, &p, is_remove_quote);
			tok; tok = strtok_pathW(NULL, separator, &p, is_remove_quote)) {
		if (RegisterPath(tok))	cnt++;
	}
	free(multi_path);
	return	cnt;
}

inline BOOL has_chars(const WCHAR *str, const WCHAR *chars)
{
	if (!str || !chars) return FALSE;

	for ( ; *chars; chars++) {
		if (wcschr(str, *chars)) return TRUE;
	}
	return FALSE;
}

int PathArray::GetMultiPath(WCHAR *multi_path, int max_len,
	const WCHAR *separator, const WCHAR *escape_chars, BOOL end_sep)
{
	int		sep_len = (int)wcslen(separator);
	int		total_len = 0;

	for (int i=0; i < num; i++) {
		auto	pa = pathArray[i];
		BOOL	is_escape = has_chars(pa->path.get(), escape_chars);
		int		need_len = pa->len + 1 + (is_escape ? 2 : 0) + (i ? sep_len : 0);

		if (max_len - (end_sep ? sep_len : 0) - total_len < need_len) {
			multi_path[total_len] = 0;
			return -1;
		}
		if (i) {
			memcpy(multi_path + total_len, separator, sep_len * sizeof(WCHAR));
			total_len += sep_len;
		}
		if (is_escape) {
			multi_path[total_len] = '"';
			total_len++;
		}
		total_len += pa->Get(multi_path + total_len);

		if (is_escape) {
			multi_path[total_len] = '"';
			total_len++;
		}
	}
	if (end_sep) {
		total_len += wcscpyz(multi_path + total_len, separator);
	}
	else {
		multi_path[total_len] = 0;
	}
	return	total_len;
}

int PathArray::GetMultiPathLen(const WCHAR *separator, const WCHAR *escape_chars, BOOL end_sep)
{
	int		total_len = 0;
	int		sep_len = (int)wcslen(separator);

	for (int i=0; i < num; i++) {
		auto	pa = pathArray[i];
		BOOL	is_escape = has_chars(pa->path.get(), escape_chars);
		total_len += pa->len + (is_escape ? 2 : 0) + (i ? sep_len : 0);
	}
	return	total_len + 1 + (end_sep ? sep_len : 0);
}

BOOL PathArray::SetPath(int idx, PathObj *obj)
{
	pathArray[idx] = obj;
	Register(pathArray[idx], pathArray[idx]->hashId);
	return	TRUE;
}

BOOL PathArray::SetPath(int idx, const WCHAR *path)
{
	pathArray[idx] = new PathObj(path, IsDirStrip());
	Register(pathArray[idx], pathArray[idx]->hashId);
	return	TRUE;
}

BOOL PathArray::RegisterPath(const WCHAR *path)
{
	if (!path || !path[0]) return	FALSE;

	auto	obj = new PathObj(path, IsDirStrip());

	if ((flags & ALLOW_SAME) == 0) {
		if (auto f = (PathObj *)Search(obj, obj->hashId)) {
			*f = *obj;
			delete obj;
			return FALSE;
		}
	}

#define MAX_ALLOC	100
	if ((num % MAX_ALLOC) == 0) {
		pathArray = (PathObj **)realloc(pathArray, (num + MAX_ALLOC) * sizeof(WCHAR *));
	}
	SetPath(num++, obj);

	return	TRUE;
}

BOOL PathArray::ReplacePath(int idx, WCHAR *new_path)
{
	if (idx >= num || idx < 0)
		return	FALSE;

	if (pathArray[idx]) {
		delete pathArray[idx];
		pathArray[idx] = NULL;
	}
	SetPath(idx, new_path);
	return	TRUE;
}

void PathArray::Sort()
{
	sort(pathArray, pathArray + num, [](const PathObj *p1, const PathObj *p2) {
		return	/*p1->isDir > p2->isDir ||
				p1->isDir == p2->isDir &&*/
				 wcscmp(p1->uppr.get(), p2->uppr.get()) < 0;
	});
}

PathArray& PathArray::operator=(const PathArray& init)
{
	if (&init == this) {
		return	*this;
	}

	Init();
	flags = init.flags;

	pathArray = (PathObj **)malloc(((((num = init.num) / MAX_ALLOC) + 1) * MAX_ALLOC)
				* sizeof(WCHAR *));

	for (int i=0; i < init.num; i++) {
		SetPath(i, new PathObj(*init.pathArray[i]));
	}

	return	*this;
}


/*=========================================================================
	DriveMng
=========================================================================*/
DriveMng::DriveMng()
{
	memset(drvID, 0, sizeof(drvID));
	*driveMap = 0;
}

DriveMng::~DriveMng()
{
	Init();
}

void DriveMng::Init(NetDrvMode mode)
{
	netDrvMode = mode;
	for (int i=0; i < MAX_DRIVES; i++) {
		DriveID &drv_id = drvID[i];
		if (drv_id.data) {
			delete [] drv_id.data;
			drv_id.data = NULL;
		}
	}
	memset(drvID, 0, sizeof(drvID));
	*driveMap = 0;
}

BOOL DriveMng::RegisterDriveID(int idx, void *data, int len)
{
	DriveID &drv_id = drvID[idx];

	 // already registered
	if (drv_id.len == len && drv_id.data && memcmp(drv_id.data, data, len) == 0) {
		return TRUE;
	}
	if (drv_id.data) {
		delete [] drv_id.data;
	}
	drv_id.data = new BYTE [len];
	memcpy(drv_id.data, data, drv_id.len = len);
	drv_id.sameDrives = 1LL << idx;
	drv_id.flags = 0;

	for (int i=0; i < MAX_DRIVES; i++) {
		if (i != idx && drvID[i].data && (drvID[i].len == drv_id.len)
		&&	memcmp(drvID[i].data, drv_id.data, drvID[i].len) == 0) {
			drvID[i].sameDrives |= drv_id.sameDrives;
			drv_id.sameDrives   |= drvID[i].sameDrives;
		}
	}

	return	TRUE;
}

void DriveMng::SetDriveFlags(int idx, DWORD flags)
{
	DriveID &drv_id = drvID[idx];
	drv_id.flags = flags;
}

uint64 DriveMng::OccupancyDrives(uint64 use_drives)
{
	uint64	total_used = use_drives;

	while (use_drives) {
		int	idx = get_ntz64(use_drives);
		if (idx >= 0 && idx < MAX_DRIVES && drvID[idx].data) {
			total_used |= drvID[idx].sameDrives;
		}
		use_drives ^= (use_drives & -(int64)use_drives);
	}
	return	total_used;
}

void DriveMng::SetDriveMap(char *_driveMap)
{
	if (strcmp(_driveMap, driveMap) == 0) return;

	DWORD	val = 1;
	strcpy(driveMap, _driveMap);

	for (char *c=driveMap; *c; c++) {
		if (*c == ',') {
			val++;
		} else {
			RegisterDriveID(DrvLetterToIndex(*c), &val, sizeof(val));
		}
	}
}

void DriveMng::ModifyNetRoot(WCHAR *root)
{
	if (netDrvMode == NET_UNC_SVRONLY) {	// サーバ名だけを切り出し
		WCHAR *p = wcschr(root+2, '\\');
		if (p) {
			*p = 0;
		}
	} else if (netDrvMode == NET_UNC_COMMON) {	//
		wcscpy(root, L"#COMMON#");
	}
	// NET_UNC_FULLVAL は何もしない
}

#include <WinIoCtl.h>
#include <Ntddscsi.h>
#include <Setupapi.h>

BOOL IsSSDDrv(const WCHAR *phys_drv) {
	auto hFile = scope_raii(::CreateFileW(phys_drv, FILE_READ_ATTRIBUTES,
		FILE_SHARE_READ|FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, 0), [](auto v) {
		::CloseHandle(v);
	});
	if (hFile == INVALID_HANDLE_VALUE) return FALSE;

	STORAGE_PROPERTY_QUERY spq = {
		StorageDeviceSeekPenaltyProperty,
		PropertyStandardQuery,
	};
	DEVICE_SEEK_PENALTY_DESCRIPTOR	spq_desc = {};
	DWORD	size = 0;

	if (!::DeviceIoControl(hFile, IOCTL_STORAGE_QUERY_PROPERTY, &spq, sizeof(spq),
		&spq_desc, sizeof(spq_desc), &size, NULL)) return FALSE;

	return	spq_desc.IncursSeekPenalty ? FALSE : TRUE;
}

int DriveMng::SetDriveID(const WCHAR *_root)
{
	if (!_root || !_root[0]) {
		return -1;
	}

	WCHAR	root[MAX_PATH];
	int		idx = -1;
	::CharUpperW(wcscpy(root, _root));

	if (root[1] == ':') {
		idx = DrvLetterToIndex(root[0]);
		if (idx < 0 || idx >= MAX_LOCAL_DRIVES) {
			return -1;
		}
	} else {
		// ネットワークドライブの場合、\\server\volume\ もしくは \\server\ （設定に依存）
		// を大文字にした文字列のハッシュ値を識別値(drvId[].data)とする
		ModifyNetRoot(root);
		uint64	hash_id = MakeHash64(root, wcslen(root) * sizeof(WCHAR));
		if ((idx = shareInfo->RegisterNetDrive(hash_id)) < 0) {
			return -1;
		}
		if (drvID[idx].len) {
			return idx; // already registerd
		}
		RegisterDriveID(idx, &hash_id, sizeof(hash_id));
		return	idx;
	}

	if (drvID[idx].len) {
		return idx; // already registerd
	}

	if (::GetDriveTypeW(root) == DRIVE_REMOTE) {
		BYTE				buf[2048];
		DWORD				size = sizeof(buf);
		REMOTE_NAME_INFOW	*pni = (REMOTE_NAME_INFOW *)buf;
		DWORD				flags = 0;

		if (::WNetGetUniversalNameW(root, REMOTE_NAME_INFO_LEVEL, pni, &size) != NO_ERROR) {
			DBG("WNetGetUniversalNameW err=%d\n", GetLastError());
			return -1;
		}
		if (pni->lpUniversalName) {
			wcscpy(root, pni->lpUniversalName);
			if (auto *p = wcsstr(pni->lpUniversalName + 2, L"\\")) {
#define DAV_NAME	L"\\DavWWWRoot\\"
				if (wcsncmp(p, DAV_NAME, wsizeof(DAV_NAME) - 1) == 0) {
					flags |= WEBDAV;
				}
			}
		} // NULL の場合、root をそのまま使う

		::CharUpperW(root);
		ModifyNetRoot(root);
		uint64	hash_id = MakeHash64(root, wcslen(root) * sizeof(WCHAR));
		RegisterDriveID(idx, &hash_id, sizeof(hash_id));

		int	net_idx = shareInfo->RegisterNetDrive(hash_id);
		if (net_idx >= 0) {
			RegisterDriveID(net_idx, &hash_id, sizeof(hash_id));
		}
		SetDriveFlags(idx, flags);
		return	idx;
	}

	WCHAR	vol_name[MAX_PATH];
	if (::GetVolumeNameForVolumeMountPointW(root, vol_name, MAX_PATH)) {
		vol_name[wcslen(vol_name) -1] = 0;
		HANDLE	hFile = ::CreateFileW(vol_name, FILE_READ_ATTRIBUTES,
								FILE_SHARE_READ|FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, 0);
		if (hFile != INVALID_HANDLE_VALUE) {
			DynBuf	buf(32 * 1024);
			VOLUME_DISK_EXTENTS &vde = *(VOLUME_DISK_EXTENTS *)buf.Buf();
			DWORD	size;
			DWORD	val = 0;
			DWORD	flags = 0;
			if (::DeviceIoControl(hFile, IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS, 0, 0,
					&vde, (DWORD)buf.Size(), &size, 0)) {
				if (vde.NumberOfDiskExtents >= 1) {
					// SetDriveMapが ID: 1～Nを利用するため、オフセット0x1000を加算
					val = vde.Extents[0].DiskNumber | 0x1000;
					//for (auto i = 0; i < vde.NumberOfDiskExtents; i++) {
					//	DBGW(L"disk id(%d / %s / %s) = %d\n",
					//		i, root, vol_name, vde.Extents[i].DiskNumber);
					//}
					swprintf(vol_name, L"\\\\.\\PhysicalDrive%d", vde.Extents[0].DiskNumber);
					if (::IsSSDDrv(vol_name)) {
						flags |= SSD;
					}
				}
			}
			else {
				DBG("IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS err=%x %s\n", GetLastError(), root);
			}
			::CloseHandle(hFile);
			if (val) {
				RegisterDriveID(idx, &val, sizeof(val));
				SetDriveFlags(idx, flags);
				return idx;
			}
		}
	}
	TRegistry	reg(HKEY_LOCAL_MACHINE);
	if (reg.OpenKey(MOUNTED_DEVICES)) {
		char	reg_path[MAX_PATH * 2];
		BYTE	buf[1024];
		int		size = sizeof(buf);
		WCHAR	*wbuf = (WCHAR *)buf;
		DWORD	val = 0;

		::sprintf(reg_path, FMT_DOSDEVICES, root[0]);
		if (reg.GetByte(reg_path, buf, &size)) {
			WCHAR	*wp;
			if (wcsncmp(wbuf, L"\\??\\", 4) == 0 && (wp = wcschr(wbuf, '#'))
				&& (wp = wcschr(wp+1, '#')) && (wp = wcschr(wp, '&'))) {
				val = wcstoul(wp+1, 0, 16);
			}
			else if (wcsncmp(wbuf, L"_??_", 4) == 0 && (wp = wcschr(wbuf, '{'))) {
				val = wcstoul(wp+1, 0, 16);
			}
			else if (wcsncmp(wbuf, L"DMIO:ID:", 8) == 0) {
				val = *(DWORD *)(wbuf + 8);
			}
			else {
				val = *(DWORD *)buf;
			}
			if (val <= 30) val |= 0x88000000;
			RegisterDriveID(idx, &val, sizeof(val));
			return	idx;
		}
	}
	RegisterDriveID(idx, "", 1);
	return	idx;
}

BOOL DriveMng::IsSameDrive(const WCHAR *_root1, const WCHAR *_root2)
{
	WCHAR	root1[MAX_PATH], root2[MAX_PATH];

	::CharUpperW(wcscpy(root1, _root1));
	::CharUpperW(wcscpy(root2, _root2));

	int	idx1 = SetDriveID(root1);
	int	idx2 = SetDriveID(root2);

	if (idx1 == idx2) {
		return TRUE;
	}
	if (idx1 < 0 || idx2 < 0) {
		return FALSE;
	}

	return	drvID[idx1].len == drvID[idx2].len
		&&	memcmp(drvID[idx1].data, drvID[idx2].data, drvID[idx1].len) == 0;
}

BOOL DriveMng::IsSSD(const WCHAR *_root)
{
	int		idx = SetDriveID(_root);
	return	(drvID[idx].flags & SSD) ? TRUE : FALSE;
}

BOOL DriveMng::IsWebDAV(const WCHAR *_root)
{
	int		idx = SetDriveID(_root);
	return	(drvID[idx].flags & WEBDAV) ? TRUE : FALSE;
}

// ワード単位ではなく、文字単位で折り返すための EDIT Control 用 CallBack
int CALLBACK EditWordBreakProcW(WCHAR *str, int cur, int len, int action)
{
	switch (action) {
	case WB_ISDELIMITER:
		return	0;
	}
	return	cur;
}

BOOL GetRootDirW(const WCHAR *path, WCHAR *root_dir)
{
	if (path[0] == '\\') {	// "\\server\volname\" 4つ目の \ を見つける
		DWORD	ch;
		int		backslash_cnt = 0, offset;

		for (offset=0; (ch = path[offset]) != 0 && backslash_cnt < 4; offset++) {
			if (ch == '\\') {
				backslash_cnt++;
			}
		}
		memcpy(root_dir, path, offset * sizeof(WCHAR));
		if (backslash_cnt < 4)					// 4つの \ がない場合は、末尾に \ を付与
			root_dir[offset++] = '\\';	// （\\server\volume など）
		root_dir[offset] = 0;	// NULL terminate
	}
	else {	// "C:\" 等
		memcpy(root_dir, path, 3 * sizeof(WCHAR));
		root_dir[3] = 0;	// NULL terminate
	}
	return	TRUE;
}

/*
	ネットワークプレースを UNC path に変換 (src == dst 可）
*/

BOOL NetPlaceConvertW(WCHAR *src, WCHAR *dst)
{
	IShellLinkW		*shellLink;
	IPersistFile	*persistFile;
	WCHAR	wDstBuf[MAX_PATH];
	WCHAR	*wSrc = src;
	WCHAR	*wDst = wDstBuf;
	BOOL	ret = FALSE;
	DWORD	attr, attr_mask = FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_READONLY;

	if ((attr = ::GetFileAttributesW(src)) == 0xffffffff || (attr & attr_mask) != attr_mask) {
		return	FALSE;	// ディレクトリかつronly でないものは関係ない
	}

	if (SUCCEEDED(CoCreateInstance(CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER,
				  IID_IShellLinkW, (void **)&shellLink))) {
		if (SUCCEEDED(shellLink->QueryInterface(IID_IPersistFile, (void **)&persistFile))) {
			if (SUCCEEDED(persistFile->Load(wSrc, STGM_READ))) {
				if (SUCCEEDED(shellLink->GetPath(wDst, MAX_PATH, 0, SLGP_UNCPRIORITY))) {
					MakePathW(dst, wDstBuf, L"");
					ret = TRUE;
				}
			}
			persistFile->Release();
		}
		shellLink->Release();
	}

	return	ret;
}

DWORD ReadReparsePoint(HANDLE hFile, void *buf, DWORD size)
{
	if (!::DeviceIoControl(hFile, FSCTL_GET_REPARSE_POINT, NULL, 0, buf, size, &size, NULL)) {
		size = 0;
	}
	return	size;
}

BOOL WriteReparsePoint(HANDLE hFile, void *buf, DWORD size)
{
	if (!::DeviceIoControl(hFile, FSCTL_SET_REPARSE_POINT, buf, size, 0, 0, &size, 0)) {
		return	0;
	}
	return	TRUE;
}

BOOL IsReparseDataSame(void *d1, void *d2)
{
	REPARSE_DATA_BUFFER *r1 = (REPARSE_DATA_BUFFER *)d1;
	REPARSE_DATA_BUFFER *r2 = (REPARSE_DATA_BUFFER *)d2;

	return	r1->ReparseTag        == r2->ReparseTag
		&&	r1->ReparseDataLength == r2->ReparseDataLength
		&&	!memcmp(&r1->GenericReparseBuffer, &r2->GenericReparseBuffer,
			r1->ReparseDataLength + (IsReparseTagMicrosoft(r1->ReparseTag) ? 0 : sizeof(GUID)));
}

BOOL DeleteReparsePoint(HANDLE hFile, void *buf)
{
	REPARSE_DATA_BUFFER	rdbuf;
	DWORD	size = IsReparseTagMicrosoft(((REPARSE_DATA_BUFFER *)buf)->ReparseTag) ?
					REPARSE_DATA_BUFFER_HEADER_SIZE : REPARSE_GUID_DATA_BUFFER_HEADER_SIZE;

	memcpy(&rdbuf, buf, size);
	rdbuf.ReparseDataLength = 0;
	return	::DeviceIoControl(hFile, FSCTL_DELETE_REPARSE_POINT, &rdbuf, size, 0, 0, &size, NULL);
}

static BOOL GetSelfSid(SID *sid, DWORD *sid_size)
{
	WCHAR	user[128]={}, sys[128]={}, domain[128]={};
	DWORD	user_size = wsizeof(user);
	DWORD	domain_size = wsizeof(domain);

	if (!::GetUserNameW(user, &user_size)) {
		return FALSE;
	}

	SID_NAME_USE snu = SidTypeUser;
	return	::LookupAccountNameW(sys, user, sid, sid_size, domain, &domain_size, &snu);
}

static ACL *MyselfAcl()
{
	BYTE	sid_buf[512];
	SID		*sid = (SID *)sid_buf;
	DWORD	size = sizeof(sid_buf);

	if (!GetSelfSid(sid, &size)) {
		return NULL;
	}

	DWORD	acl_size = 512; // 手抜き
	ACL		*acl = (ACL *)malloc(acl_size);

	::InitializeAcl(acl, acl_size, ACL_REVISION);
	::AddAccessAllowedAce(acl, ACL_REVISION, GENERIC_ALL, sid);

	return	acl;
}

BOOL ResetAcl(const WCHAR *path, BOOL myself_acl)
{
	static ACL	default_acl;
	static BOOL	once_result = ::InitializeAcl(&default_acl, sizeof(default_acl), ACL_REVISION);

	ACL	*acl = once_result ? &default_acl : NULL;

	if (myself_acl) {
		static ACL	*local_acl = MyselfAcl();
		if (local_acl) acl = local_acl;
	}
	if (!acl) {
		return FALSE;
	}

	return	::SetNamedSecurityInfoW((WCHAR *)path, SE_FILE_OBJECT, DACL_SECURITY_INFORMATION |
		UNPROTECTED_DACL_SECURITY_INFORMATION, 0, 0, acl, 0) == ERROR_SUCCESS;
}

BOOL ForceRemoveDirectoryW(const WCHAR *path, DWORD flags)
{
	if (::RemoveDirectoryW(path)) {
		return TRUE;
	}
	if (flags == 0) {
		return FALSE;
	}

	DWORD	err = ::GetLastError();
	if ((flags & FMF_EMPTY_RETRY) && err == ERROR_DIR_NOT_EMPTY) {
		HANDLE	fh;
		BOOL	ret = FALSE;
		DWORD	wso = 0;

		if ((fh = ::FindFirstChangeNotificationW(path, TRUE, FILE_NOTIFY_CHANGE_FILE_NAME
			|FILE_NOTIFY_CHANGE_DIR_NAME)) != INVALID_HANDLE_VALUE) {
		}
		if (::RemoveDirectoryW(path)) {
			ret = TRUE;
		}
		else {
			for (int i=0; i < 10; i++) {
				wso = ::WaitForSingleObject(fh, 100);
				if (::RemoveDirectoryW(path)) {
					ret = TRUE;
					DBGW(L"ForceRemoveDirectoryW2 cnt=%d wso=%d\n", i, wso);
					break;
				}
				if (::GetLastError() != ERROR_DIR_NOT_EMPTY) {
					break;
				}
				if (wso == WAIT_TIMEOUT) {
					break;
				}
				::FindNextChangeNotification(fh);
			}
		}
		::FindCloseChangeNotification(fh);

		DBGW(L"ForceRemoveDirectoryW notify %d wso=%x\n", ret, wso);

		return	ret ? ret : ::RemoveDirectoryW(path);
	}

	if (err != ERROR_ACCESS_DENIED) {
		return FALSE;
	}

	if (flags & FMF_ACL) {
		flags &= ~FMF_ACL;
		ResetAcl(path);
		if (::RemoveDirectoryW(path)) {
			return TRUE;
		}
		if (flags == 0 || ::GetLastError() != ERROR_ACCESS_DENIED) {
			return FALSE;
		}
	}
	if (flags & FMF_MYACL) {
		flags &= ~FMF_MYACL;
		ResetAcl(path, TRUE);
		if (::RemoveDirectoryW(path)) {
			return TRUE;
		}
		if (flags == 0 || ::GetLastError() != ERROR_ACCESS_DENIED) {
			return FALSE;
		}
	}
	if (flags & FMF_ATTR) {
		flags &= ~FMF_ATTR;
		::SetFileAttributesW(path, FILE_ATTRIBUTE_DIRECTORY);
		if (::RemoveDirectoryW(path)) {
			return TRUE;
		}
		if (flags == 0 || ::GetLastError() != ERROR_ACCESS_DENIED) {
			return FALSE;
		}
	}
	return	FALSE;
}

BOOL ForceDeleteFileW(const WCHAR *path, DWORD flags)
{
	if (::DeleteFileW(path)) {
		return TRUE;
	}
	if (flags == 0 || ::GetLastError() != ERROR_ACCESS_DENIED) {
		return FALSE;
	}

	if (flags & FMF_ACL) {
		flags &= ~FMF_ACL;
		ResetAcl(path);
		if (::DeleteFileW(path)) {
			return TRUE;
		}
		if (flags == 0 || ::GetLastError() != ERROR_ACCESS_DENIED) {
			return FALSE;
		}
	}
	if (flags & FMF_MYACL) {
		flags &= ~FMF_MYACL;
		ResetAcl(path, TRUE);
		if (flags == 0 || ::GetLastError() != ERROR_ACCESS_DENIED) {
			return FALSE;
		}
	}
	if (flags & FMF_ATTR) {
		flags &= ~FMF_ATTR;
		::SetFileAttributesW(path, FILE_ATTRIBUTE_NORMAL);
		if (::DeleteFileW(path)) {
			return TRUE;
		}
		if (flags == 0 || ::GetLastError() != ERROR_ACCESS_DENIED) {
			return FALSE;
		}
	}
	return	FALSE;
}

HANDLE ForceCreateFileW(const WCHAR *path, DWORD mode, DWORD share, SECURITY_ATTRIBUTES *sa,
	DWORD cr_mode, DWORD cr_flg, HANDLE hTempl, DWORD flags)
{
	HANDLE	fh = ::CreateFileW(path, mode, share, sa, cr_mode, cr_flg, hTempl);
	if (fh != INVALID_HANDLE_VALUE) {
		return fh;
	}
	if (flags == 0 || ::GetLastError() != ERROR_ACCESS_DENIED) {
		return INVALID_HANDLE_VALUE;
	}

	if (flags & FMF_ACL) {
		flags &= ~FMF_ACL;
		ResetAcl(path);
		fh = ::CreateFileW(path, mode, share, sa, cr_mode, cr_flg, hTempl);
		if (fh != INVALID_HANDLE_VALUE) {
			return fh;
		}
		if (flags == 0 || ::GetLastError() != ERROR_ACCESS_DENIED) {
			return INVALID_HANDLE_VALUE;
		}
	}
	if (flags & FMF_MYACL) {
		flags &= ~FMF_MYACL;
		ResetAcl(path, TRUE);
		fh = ::CreateFileW(path, mode, share, sa, cr_mode, cr_flg, hTempl);
		if (fh != INVALID_HANDLE_VALUE) {
			return fh;
		}
		if (flags == 0 || ::GetLastError() != ERROR_ACCESS_DENIED) {
			return INVALID_HANDLE_VALUE;
		}
	}
	if (flags & FMF_ATTR) {
		flags &= ~FMF_ATTR;
		::SetFileAttributesW(path, FILE_ATTRIBUTE_NORMAL); // DIRの場合も ronlyは消える
		fh = ::CreateFileW(path, mode, share, sa, cr_mode, cr_flg, hTempl);
		if (fh != INVALID_HANDLE_VALUE) {
			return fh;
		}
		if (flags == 0 || ::GetLastError() != ERROR_ACCESS_DENIED) {
			return INVALID_HANDLE_VALUE;
		}
	}
	return	fh;
}

/*
 DataList ...  List with data(hash/fileID...)
*/
DataList::DataList(ssize_t size, ssize_t max_size, ssize_t _grow_size, ssize_t _min_margin)
{
	num = 0;
	top = end = NULL;

	if (size) Init(size, max_size, _grow_size, _min_margin);
}

DataList::~DataList()
{
	UnInit();
}

BOOL DataList::Init(ssize_t size, ssize_t max_size, ssize_t _grow_size, ssize_t _min_margin)
{
	grow_size = _grow_size;
	min_margin = _min_margin;
	cv.Initialize();

	BOOL ret = buf.AllocBuf(size, max_size);
	Clear();
	return	ret;
}

void DataList::UnInit()
{
	top = end = NULL;
	buf.FreeBuf();
	cv.UnInitialize();
}

void DataList::Clear()
{
	top = end = NULL;
	buf.SetUsedSize(0);
	num = 0;
}

DataList::Head *DataList::Alloc(void *data, ssize_t data_size, ssize_t need_size)
{
	Head	*cur = NULL;
	ssize_t	alloc_size = need_size + sizeof(Head);

	alloc_size = ALIGN_SIZE(alloc_size, 8);

	if (!top) {
		cur = top = end = (Head *)buf.Buf();
		cur->next = cur->prev = NULL;
	}
	else {
		if (top >= end) {
			cur = (Head *)((BYTE *)top + top->alloc_size);
			if ((BYTE *)cur + alloc_size < buf.Buf() + buf.MaxSize()) {
				ssize_t need_grow = ((BYTE *)cur + alloc_size) - (buf.Buf() + buf.Size());
				if (need_grow > 0) {
					if (!buf.Grow(ALIGN_SIZE(need_grow, PAGE_SIZE))) {
						//MessageBox(0, "can't alloc mem", "", MB_OK);
						return	NULL;
					}
				}
			}
			else {
				if ((BYTE *)end < buf.Buf() + alloc_size) {	// for debug
					//MessageBox(0, "buf is too small", "", MB_OK);
					return	NULL;
				}
				cur = (Head *)buf.Buf();
			}
		}
		else {
			if ((BYTE *)end < (BYTE *)top + top->alloc_size + alloc_size) {	// for debug
				//MessageBox(0, "buf is too small2", "", MB_OK);
				return	NULL;
			}
			cur = (Head *)((BYTE *)top + top->alloc_size);
		}
		//Head *buf_end = (Head *)(buf.Buf() + buf.Size());
		//if (buf_end < cur || buf_end < top || cur < (Head *)buf.Buf()) {
		//	MessageBox(0, "illigal datalist", "", MB_OK);
		//	goto END;
		//}

		top->next = cur;
		cur->prev = top;
		cur->next = NULL;
		top = cur;
	}
	cur->alloc_size = alloc_size;
	cur->data_size = data_size;
	if (data) {
		memcpy(cur->data, data, data_size);
	}
	num++;

	return	cur;
}

DataList::Head *DataList::Get()
{
	Head	*cur = end;

	if (!cur) goto END;

	if (cur->next) {
		cur->next->prev = cur->prev;
	}
	else {
		top = cur->prev;

		//if (top && top < (Head *)buf.Buf()) {
		//	MessageBox(0, "illigal datalist", "", MB_OK);
		//	goto END;
		//}
	}
	end = cur->next;

	//if (end && end < (Head *)buf.Buf()) {
	//	MessageBox(0, "illigal datalist", "", MB_OK);
	//	goto END;
	//}

	num--;

END:
	return	cur;
}

DataList::Head *DataList::Peek(Head *prev)
{
	Head	*cur = prev ? prev->next : end;

	return	cur;
}

ssize_t DataList::RemainSize()
{
	ssize_t ret = 0;

	if (top) {
		BYTE *top_end = (BYTE *)top + top->alloc_size;

		if (top >= end) {
			ssize_t size1 = buf.MaxSize() - (top_end - buf.Buf());
			ssize_t size2 = (BYTE *)end - buf.Buf();
			ret = max(size1, size2);
		}
		else {
			ret = (BYTE *)end - top_end;
		}
	}
	else {
		ret = buf.MaxSize();
	}

	ret -= sizeof(Head);

	if (ret < 0) {
		ret = 0;
	}

	return	ret;
}

ssize_t comma_int64(WCHAR *s, int64 val)
{
	WCHAR	tmp[40], *sv_s = s;
	ssize_t	len = swprintf(tmp, L"%lld", val);

	for (WCHAR *p=tmp; *s++ = *p++; ) {
		if (len > 2 && (--len % 3) == 0) *s++ = ',';
	}
	return	s - sv_s - 1;
}

ssize_t comma_double(WCHAR *s, double val, int precision)
{
	WCHAR	tmp[40], *sv_s = s;
	ssize_t	len = swprintf(tmp, L"%.*f", precision, val);
	WCHAR	*pos = precision ? wcschr(tmp, '.') : NULL;

	if (pos) len = pos - tmp;

	for (WCHAR *p=tmp; *s++ = *p++; ) {
		if ((!pos || p < pos) && len > 2 && (--len % 3) == 0) *s++ = ',';
	}
	return	s - sv_s - 1;
}

ssize_t comma_int64(char *s, int64 val)
{
	char	tmp[40], *sv_s = s;
	ssize_t	len = sprintf(tmp, "%lld", val);

	for (char *p=tmp; *s++ = *p++; ) {
		if (len > 2 && (--len % 3) == 0) *s++ = ',';
	}
	return	s - sv_s - 1;
}

ssize_t comma_double(char *s, double val, int precision)
{
	char	tmp[40], *sv_s = s;
	ssize_t	len = sprintf(tmp, "%.*f", precision, val);
	char	*pos = precision ? strchr(tmp, '.') : NULL;

	if (pos) len = pos - tmp;

	for (char *p=tmp; *s++ = *p++; ) {
		if ((!pos || p < pos) && len > 2 && (--len % 3) == 0) *s++ = ',';
	}
	return	s - sv_s - 1;
}

void ShowHelp(const WCHAR *dir, const WCHAR *file, const WCHAR *section)
{
	if (wcsstr(file, L".chm::")) {
		ShowHelpW(0, dir, file, section);
		return;
	}

	WCHAR	app[MAX_PATH] = {};
	WCHAR	path[MAX_PATH] = {};

	auto	len = snwprintfz(path, wsizeof(path),
								L"file:///%s/%s%s", dir, file, section ? section : L"");

	ReplaceCharW(path, '\\', '/', MAX_PATH);


	if (TGetUrlAssocAppW(L"https", app, wsizeof(app))) {
		ShellExecuteW(0, 0, app, path, 0, SW_SHOW);
	}
	else {
		ShellExecuteW(0, 0, path, 0, 0, SW_SHOW);
	}
}


#ifdef DEBUG
void DBGWrite(char *fmt,...)
{
	static HANDLE	hDbgFile = INVALID_HANDLE_VALUE;

	if (hDbgFile == INVALID_HANDLE_VALUE) {
		hDbgFile = ::CreateFile("c:\\tlib_dbg.txt",
					GENERIC_WRITE, FILE_SHARE_READ|FILE_SHARE_WRITE, 0, CREATE_ALWAYS, 0, 0);
	}

	char	buf[1024];
	va_list	va;
	va_start(va, fmt);
	DWORD	len = wvsprintf(buf, fmt, va);
	va_end(va);
	::WriteFile(hDbgFile, buf, len, &len, 0);
}

void DBGWriteW(WCHAR *fmt,...)
{
	static HANDLE	hDbgFile = INVALID_HANDLE_VALUE;

	if (hDbgFile == INVALID_HANDLE_VALUE) {
		hDbgFile = ::CreateFile("c:\\tlib_dbgw.txt",
					GENERIC_WRITE, FILE_SHARE_READ|FILE_SHARE_WRITE, 0, CREATE_ALWAYS, 0, 0);
		DWORD	len = 2;
		::WriteFile(hDbgFile, "\xff\xfe", len, &len, 0);
	}

	WCHAR	buf[1024];
	va_list	va;
	va_start(va, fmt);
	DWORD	len = vswprintf(buf, fmt, va);
	va_end(va);
	::WriteFile(hDbgFile, buf, len * 2, &len, 0);
}
#endif


#ifdef TRACE_DBG
#define MAX_LOG 8192
static WCHAR logbuf[MAX_LOG][256];
static DWORD logbuf_idx;
static DWORD logbuf_nest;

static BOOL trclog_firstinit(CRITICAL_SECTION *cs)
{
	InitializeCriticalSection(cs);
	logbuf_nest = ::TlsAlloc();
	return TRUE;
}

void trclog_init()
{
	memset(logbuf, 0, sizeof(logbuf));
	logbuf_idx = 0;
	logbuf_nest = 0;
}

void trclog(const WCHAR *func, int lines)
{
	static CRITICAL_SECTION	cs;
	static BOOL		firstInit = trclog_firstinit(&cs);

	::EnterCriticalSection(&cs);

	DWORD nest = (DWORD)(DWORD_PTR)::TlsGetValue(logbuf_nest);

	if (lines == 0 && nest > 0) {
		nest--;
	}

	WCHAR *targ = logbuf[logbuf_idx++ % MAX_LOG];
	_snwprintf(targ, MAX_LOG, L"%05x:%.*s %s%s:%d", GetCurrentThreadId(), min(nest, 30),
		L"                              ", lines ? L"" : L"-- end of ", func, lines);

	if (lines) {
		nest++;
	}
	::TlsSetValue(logbuf_nest, (LPVOID)(DWORD_PTR)nest);

	::LeaveCriticalSection(&cs);
}

WCHAR *trclog_get(DWORD idx)
{
	if (idx >= MAX_LOG) {
		return	NULL;
	}
	return	logbuf[(logbuf_idx + idx) % MAX_LOG];
}

#endif

