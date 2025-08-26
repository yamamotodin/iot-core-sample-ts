#!/bin/bash

openssl genrsa -out device-private.key 2048
openssl req -new -key device-private.key -out device.csr -subj "/CN=MyIoTDevice01"

