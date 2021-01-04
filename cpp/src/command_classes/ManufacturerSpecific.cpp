//-----------------------------------------------------------------------------
//
//	ManufacturerSpecific.cpp
//
//	Implementation of the Z-Wave COMMAND_CLASS_MANUFACTURER_SPECIFIC
//
//	Copyright (c) 2010 Mal Lansell <openzwave@lansell.org>
//
//	SOFTWARE NOTICE AND LICENSE
//
//	This file is part of OpenZWave.
//
//	OpenZWave is free software: you can redistribute it and/or modify
//	it under the terms of the GNU Lesser General Public License as published
//	by the Free Software Foundation, either version 3 of the License,
//	or (at your option) any later version.
//
//	OpenZWave is distributed in the hope that it will be useful,
//	but WITHOUT ANY WARRANTY; without even the implied warranty of
//	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//	GNU Lesser General Public License for more details.
//
//	You should have received a copy of the GNU Lesser General Public License
//	along with OpenZWave.  If not, see <http://www.gnu.org/licenses/>.
//
//-----------------------------------------------------------------------------

#include "command_classes/CommandClasses.h"
#include "command_classes/ManufacturerSpecific.h"
#include "tinyxml.h"

#include "Defs.h"
#include "Msg.h"
#include "Node.h"
#include "Options.h"
#include "Manager.h"
#include "Driver.h"
#include "ManufacturerSpecificDB.h"
#include "Notification.h"
#include "platform/Log.h"

#include "value_classes/ValueStore.h"
#include "value_classes/ValueString.h"
#include "value_classes/ValueInt.h"

namespace OpenZWave
{
	namespace Internal
	{
		namespace CC
		{

//-----------------------------------------------------------------------------
// <ManufacturerSpecific::ManufacturerSpecific>
// Constructor
//-----------------------------------------------------------------------------
			ManufacturerSpecific::ManufacturerSpecific(uint32 const _homeId, uint8 const _nodeId) :
					CommandClass(_homeId, _nodeId), m_fileConfigRevision(0), m_loadedConfigRevision(0), m_latestConfigRevision(0)
			{
				SetStaticRequest(StaticRequest_Values);
			}

//-----------------------------------------------------------------------------
// <ManufacturerSpecific::RequestState>
// Request current state from the device
//-----------------------------------------------------------------------------
			bool ManufacturerSpecific::RequestState(uint32 const _requestFlags, uint8 const _instance, Driver::MsgQueue const _queue)
			{
				// nothing to do, Z-Way is requesting that data during interview
				return true;
			}

			bool ManufacturerSpecific::Init() {
				if (m_com.GetFlagBool(COMPAT_FLAG_GETSUPPORTED))
				{
					// nothing to do, Z-Way is requesting that data during interview
					return true;
				}
				else
				{
					Log::Write(LogLevel_Info, GetNodeId(), "ManufacturerSpecificCmd_Get Not Supported on this node");
					return true;
				}
				return false;
			}

			void ManufacturerSpecific::SetProductDetails(uint16 manufacturerId, uint16 productType, uint16 productId)
			{

				string configPath = "";
				
				TODO(Remove OZW XML files?)
				std::shared_ptr<Internal::ProductDescriptor> product = NULL; // GetDriver()->GetManufacturerSpecificDB()->getProduct(manufacturerId, productType, productId);

				Node *node = GetNodeUnsafe();
				if (!product)
				{
					char str[64];
					snprintf(str, sizeof(str), "Unknown: id=%.4x", manufacturerId);
					string manufacturerName = str;

					snprintf(str, sizeof(str), "Unknown: type=%.4x, id=%.4x", productType, productId);
					string productName = str;

					node->SetManufacturerName(manufacturerName);
					node->SetProductName(productName);
				}
				else
				{
					node->SetManufacturerName(product->GetManufacturerName());
					node->SetProductName(product->GetProductName());
					node->SetProductDetails(product);
				}

				node->SetManufacturerId(manufacturerId);

				node->SetProductType(productType);

				node->SetProductId(productId);

			}


			
//-----------------------------------------------------------------------------
// <ManufacturerSpecific::SetManufacturerProductId>
// Set manufacturer, product type and id
//-----------------------------------------------------------------------------
			void ManufacturerSpecific::SetManufacturerProductId(uint8 instance)
			{
				zdata_acquire_lock((ZDataRootObject)GetZWay());
				_SetManufacturerProductId(instance);
				zdata_release_lock((ZDataRootObject)GetZWay());
			}
//-----------------------------------------------------------------------------
// <ManufacturerSpecific::_SetManufacturerProductId>
// Set manufacturer, product type and id
//-----------------------------------------------------------------------------
			void ManufacturerSpecific::_SetManufacturerProductId(uint8 instance)
			{
				LOG_CALL
				
				Node* node = GetNodeUnsafe();
				
				int manufacturerId, productType, productId;
				
				ZWay zway = GetZWay();
				LOG_ERR(zdata_get_integer(zway_find_device_instance_cc_data(zway, GetNodeId(), instance - 1, StaticGetCommandClassId(), "vendorId"), &manufacturerId));
				LOG_ERR(zdata_get_integer(zway_find_device_instance_cc_data(zway, GetNodeId(), instance - 1, StaticGetCommandClassId(), "productType"), &productType));
				LOG_ERR(zdata_get_integer(zway_find_device_instance_cc_data(zway, GetNodeId(), instance - 1, StaticGetCommandClassId(), "productId"), &productId));
				
				if (node)
				{
					// Attempt to create the config parameters
					SetProductDetails(manufacturerId, productType, productId);
					ClearStaticRequest(StaticRequest_Values);
					node->m_manufacturerSpecificClassReceived = true;

					if (node->getConfigPath().size() > 0)
					{
						LoadConfigXML();
					}

					Log::Write(LogLevel_Info, GetNodeId(), "Received manufacturer specific report from node %d: Manufacturer=%s, Product=%s", GetNodeId(), node->GetManufacturerName().c_str(), node->GetProductName().c_str());
					Log::Write(LogLevel_Info, GetNodeId(), "Node Identity Codes: %.4x:%.4x:%.4x", manufacturerId, productType, productId);
				}

				// Notify the watchers of the name changes
				Notification* notification = new Notification(Notification::Type_NodeNaming);
				notification->SetHomeAndNodeIds(GetHomeId(), GetNodeId());
				GetDriver()->QueueNotification(notification);
			}
			
//-----------------------------------------------------------------------------
// <ManufacturerSpecific::Watcher>
// Handles Z-Way events
//-----------------------------------------------------------------------------
			void ManufacturerSpecific::Watcher(ZWDataChangeType type, ZDataHolder data, uint8 instance)
			{
				LOG_CALL
				
				ZWay zway = GetZWay();
				Node* node = GetNodeUnsafe();
				
				if ((type & ~PhantomUpdate) == Updated)
				{
					if (strcmp(zdata_get_path(data), "productId") == 0)
					{
						_SetManufacturerProductId(instance); // zdata is locked here
					}
					
					if (strcmp(zdata_get_path(data), "serialNumber") == 0)
					{
						ZWCSTR deviceID_c_str;
						const ZWBYTE *deviceID_c_bin;
						size_t deviceID_c_len;
						string deviceID = "";
						
						ZWDataType type;
						LOG_ERR(zdata_get_type(zway_find_device_instance_cc_data(zway, GetNodeId(), instance - 1, StaticGetCommandClassId(), "serialNumber"), &type));
						switch (type)
						{
							case Binary:
								LOG_ERR(zdata_get_binary(zway_find_device_instance_cc_data(zway, GetNodeId(), instance - 1, StaticGetCommandClassId(), "serialNumber"), &deviceID_c_bin, &deviceID_c_len));
								for (size_t i = 0; i < deviceID_c_len; i++)
								{
									char temp_chr[3];
									snprintf(temp_chr, sizeof(temp_chr), "%.2x", deviceID_c_bin[i]);
									deviceID += temp_chr;
								}
								break;
							case String:
								LOG_ERR(zdata_get_string(zway_find_device_instance_cc_data(zway, GetNodeId(), instance - 1, StaticGetCommandClassId(), "serialNumber"), &deviceID_c_str));
								deviceID = deviceID_c_str;
								break;
							default:
								break;
						}
						
						if (deviceID != "" && node)
						{
							if (!GetValue(instance, ValueID_Index_ManufacturerSpecific::SerialNumber)) {
								node->CreateValueString(ValueID::ValueGenre_System, StaticGetCommandClassId(), instance, ValueID_Index_ManufacturerSpecific::SerialNumber, "Serial Number", "", true, false, "", 0);
							}
							Internal::VC::ValueString *serial_value = static_cast<Internal::VC::ValueString*>(GetValue(instance, ValueID_Index_ManufacturerSpecific::SerialNumber));
							serial_value->OnValueRefreshed(deviceID);
							serial_value->Release();
							Log::Write(LogLevel_Info, GetNodeId(), "Got ManufacturerSpecific SerialNumber: %s", deviceID.c_str());
						}
					}
				}
			}
			

//-----------------------------------------------------------------------------
// <ManufacturerSpecific::LoadConfigXML>
// Try to find and load an XML file describing the device's config params
//-----------------------------------------------------------------------------
			bool ManufacturerSpecific::LoadConfigXML()
			{
				if (GetNodeUnsafe()->getConfigPath().size() == 0)
					return false;

				string configPath;
				Options::Get()->GetOptionAsString("ConfigPath", &configPath);

				string filename = configPath + GetNodeUnsafe()->getConfigPath();

				TiXmlDocument* doc = new TiXmlDocument();
				Log::Write(LogLevel_Info, GetNodeId(), "  Opening config param file %s", filename.c_str());
				if (!doc->LoadFile(filename.c_str(), TIXML_ENCODING_UTF8))
				{
					delete doc;
					Log::Write(LogLevel_Info, GetNodeId(), "Unable to find or load Config Param file %s", filename.c_str());
					return false;
				}
				doc->SetUserData((void *) filename.c_str());
				/* make sure it has the right xmlns */
				TiXmlElement *product = doc->RootElement();
				char const *xmlns = product->Attribute("xmlns");
				if (xmlns && strcmp(xmlns, "https://github.com/OpenZWave/open-zwave"))
				{
					delete doc;
					Log::Write(LogLevel_Warning, GetNodeId(), "Invalid XML Namespace in %s - Ignoring", filename.c_str());
					return false;
				}

				Node::QueryStage qs = GetNodeUnsafe()->GetCurrentQueryStage();
				if (qs == Node::QueryStage_ManufacturerSpecific1)
				{
					GetNodeUnsafe()->ReadDeviceProtocolXML(doc->RootElement());
				}
				else
				{
					if (!GetNodeUnsafe()->m_manufacturerSpecificClassReceived)
					{
						GetNodeUnsafe()->ReadDeviceProtocolXML(doc->RootElement());
					}
				}
				GetNodeUnsafe()->ReadCommandClassesXML(doc->RootElement());
				GetNodeUnsafe()->ReadMetaDataFromXML(doc->RootElement());
				delete doc;
				return true;
			}

//-----------------------------------------------------------------------------
// <ManufacturerSpecific::ReLoadConfigXML>
// Reload previously discovered device configuration.
//-----------------------------------------------------------------------------
			void ManufacturerSpecific::ReLoadConfigXML()
			{
				LoadConfigXML();
			}

//-----------------------------------------------------------------------------
// <ManufacturerSpecific::CreateVars>
// Create the values managed by this command class
//-----------------------------------------------------------------------------
			void ManufacturerSpecific::CreateVars(uint8 const _instance)
			{
				if (_instance == 1)
				{
					if (Node* node = GetNodeUnsafe())
					{
						node->CreateValueInt(ValueID::ValueGenre_System, GetCommandClassId(), _instance, ValueID_Index_ManufacturerSpecific::LoadedConfig, "Loaded Config Revision", "", true, false, m_loadedConfigRevision, 0);
						node->CreateValueInt(ValueID::ValueGenre_System, GetCommandClassId(), _instance, ValueID_Index_ManufacturerSpecific::LocalConfig, "Config File Revision", "", true, false, m_fileConfigRevision, 0);
						node->CreateValueInt(ValueID::ValueGenre_System, GetCommandClassId(), _instance, ValueID_Index_ManufacturerSpecific::LatestConfig, "Latest Available Config File Revision", "", true, false, m_latestConfigRevision, 0);
						
						AddWatcher(_instance, "productId");
						AddWatcher(_instance, "serialNumber");
						
						// set product ID on load
						SetManufacturerProductId(_instance);
					}
				}
				
				
			}

//-----------------------------------------------------------------------------
// <ManufacturerSpecific::setLatestRevision>
// Set the Latest Config Revision Available for this device
//-----------------------------------------------------------------------------
			void ManufacturerSpecific::setLatestConfigRevision(uint32 rev)
			{

				m_latestConfigRevision = rev;

				if (Internal::VC::ValueInt* value = static_cast<Internal::VC::ValueInt*>(GetValue(1, ValueID_Index_ManufacturerSpecific::LatestConfig)))
				{
					value->OnValueRefreshed(rev);
					value->Release();
				}

			}

//-----------------------------------------------------------------------------
// <ManufacturerSpecific::setFileConfigRevision>
// Set the File Config Revision for this device
//-----------------------------------------------------------------------------

			void ManufacturerSpecific::setFileConfigRevision(uint32 rev)
			{
				m_fileConfigRevision = rev;

				if (Internal::VC::ValueInt* value = static_cast<Internal::VC::ValueInt*>(GetValue(1, ValueID_Index_ManufacturerSpecific::LocalConfig)))
				{
					value->OnValueRefreshed(rev);
					value->Release();
				}

			}

//-----------------------------------------------------------------------------
// <ManufacturerSpecific::setFileConfigRevision>
// Set the File Config Revision for this device
//-----------------------------------------------------------------------------

			void ManufacturerSpecific::setLoadedConfigRevision(uint32 rev)
			{
				m_loadedConfigRevision = rev;

				if (Internal::VC::ValueInt* value = static_cast<Internal::VC::ValueInt*>(GetValue(1, ValueID_Index_ManufacturerSpecific::LoadedConfig)))
				{
					value->OnValueRefreshed(rev);
					value->Release();
				}
			}
		} // namespace CC
	} // namespace Internal
} // namespace OpenZWave
