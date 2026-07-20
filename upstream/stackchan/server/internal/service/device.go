/*
SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
SPDX-License-Identifier: MIT
*/

package service

import (
	"context"
	"stackChan/internal/dao"
	"stackChan/internal/model/do"
	"stackChan/internal/model/entity"

	"github.com/gogf/gf/v2/database/gdb"
	"github.com/gogf/gf/v2/errors/gcode"
	"github.com/gogf/gf/v2/errors/gerror"
)

func CreateMacIfNotExists(ctx context.Context, mac string) (id int64, err error) {
	if err = validateMac(mac); err != nil {
		return 0, err
	}

	_, err = dao.Device.Ctx(ctx).Data(do.Device{
		Mac: mac,
	}).InsertIgnore()
	if err != nil {
		return 0, err
	}
	return 0, nil
}

func CreateMacIfNotExistsWithTx(ctx context.Context, tx gdb.TX, mac string) error {
	if err := validateMac(mac); err != nil {
		return err
	}

	_, err := dao.Device.Ctx(ctx).TX(tx).Data(do.Device{
		Mac: mac,
	}).InsertIgnore()
	return err
}

func validateMac(mac string) error {
	if mac == "" {
		return gerror.NewCode(gcode.CodeMissingParameter, "Device MAC address cannot be empty")
	}
	return nil
}

func GetDeviceName(ctx context.Context, mac string) (name string, err error) {
	var device entity.Device
	err = dao.Device.Ctx(ctx).Where("mac = ?", mac).Fields("name").Scan(&device)
	if err != nil {
		return "", err
	}
	return device.Name, nil
}
