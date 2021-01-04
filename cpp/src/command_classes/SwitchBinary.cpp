//-----------------------------------------------------------------------------
//
//	SwitchBinary.cpp
//
//	Implementation of the Z-Wave COMMAND_CLASS_SWITCH_BINARY
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
//  spec: http://zwavepublic.com/sites/default/files/command_class_specs_2017A/SDS13781-3%20Z-Wave%20Application%20Command%20Class%20Specification.pdf
//        pp. 78ff
//-----------------------------------------------------------------------------

#include "command_classes/CommandClasses.h"
#include "command_classes/SwitchBinary.h"
#include "command_classes/WakeUp.h"
#include "Defs.h"
#include "Msg.h"
#include "Driver.h"
#include "Node.h"
#include "platform/Log.h"

#include "value_classes/ValueBool.h"
#include "value_classes/ValueInt.h"

namespace OpenZWave
{
	namespace Internal
	{
		namespace CC
		{

//-----------------------------------------------------------------------------
// <SwitchBinary::RequestState>
// Request current state from the device
//-----------------------------------------------------------------------------
			bool SwitchBinary::RequestState(uint32 const _requestFlags, uint8 const _instance, Driver::MsgQueue const _queue)
			{
				if (_requestFlags & RequestFlag_Dynamic)
				{
					return RequestValue(_requestFlags, ValueID_Index_SwitchBinary::Level, _instance, _queue);
				}

				return false;
			}

//-----------------------------------------------------------------------------
// <SwitchBinary::RequestValue>
// Request current value from the device
//-----------------------------------------------------------------------------
			bool SwitchBinary::RequestValue(uint32 const _requestFlags, uint16 const _dummy1,	// = 0 (not used)
					uint8 const _instance, Driver::MsgQueue const _queue)
			{
				if (m_com.GetFlagBool(COMPAT_FLAG_GETSUPPORTED))
				{
					return NoError == zway_cc_switch_binary_get(GetDriver()->GetZWay(), GetNodeId(), _instance - 1, NULL, NULL, NULL);
				}
				else
				{
					Log::Write(LogLevel_Info, GetNodeId(), "SwitchBinaryCmd_Get Not Supported on this node");
				}
				return false;
			}

//-----------------------------------------------------------------------------
// <SwitchBinary::SetValue>
// Set the state of the switch
//-----------------------------------------------------------------------------
			bool SwitchBinary::SetValue(Internal::VC::Value const& _value)
			{
				bool res = false;
				uint8 instance = _value.GetID().GetInstance();

				switch (_value.GetID().GetIndex())
				{
					case ValueID_Index_SwitchBinary::Level:
					{
						if (Internal::VC::ValueBool* value = static_cast<Internal::VC::ValueBool*>(GetValue(instance, ValueID_Index_SwitchBinary::Level)))
						{
							res = SetState(instance, (static_cast<Internal::VC::ValueBool const*>(&_value))->GetValue());
							value->Release();
						}
						break;
					}
					case ValueID_Index_SwitchBinary::Duration:
					{
						if (Internal::VC::ValueInt* value = static_cast<Internal::VC::ValueInt*>(GetValue(instance, ValueID_Index_SwitchBinary::Duration)))
						{
							value->OnValueRefreshed((static_cast<Internal::VC::ValueInt const*>(&_value))->GetValue());
							value->Release();
						}
						res = true;
						break;
					}

				}

				return res;
			}

//-----------------------------------------------------------------------------
// <SwitchBinary::SetValueBasic>
// Update class values based in BASIC mapping
//-----------------------------------------------------------------------------
			void SwitchBinary::SetValueBasic(uint8 const _instance, uint8 const _value)
			{
				if (Node* node = GetNodeUnsafe())
				{
					(void)node; TODO(why do we need this node?)
					
					if (Internal::VC::ValueBool* value = static_cast<Internal::VC::ValueBool*>(GetValue(_instance, 0)))
					{
						value->OnValueRefreshed(_value != 0);
						value->Release();
					}
				}
			}

//-----------------------------------------------------------------------------
// <SwitchBinary::SetState>
// Set a new state for the switch
//-----------------------------------------------------------------------------
			bool SwitchBinary::SetState(uint8 const _instance, bool const _state)
			{
				uint8 const targetValue = _state ? 0xff : 0;

				if (GetVersion() >= 2)
				{
					Internal::VC::ValueInt* durationValue = static_cast<Internal::VC::ValueInt*>(GetValue(_instance, ValueID_Index_SwitchBinary::Duration));
					uint32 duration = durationValue->GetValue();
					durationValue->Release();
					if (duration > 7620)
						Log::Write(LogLevel_Info, GetNodeId(), "  Duration: Device Default");
					else if (duration > 0x7F)
						Log::Write(LogLevel_Info, GetNodeId(), "  Rouding to %d Minutes (over 127 seconds)", encodeDuration(duration)-0x79);
					else 
						Log::Write(LogLevel_Info, GetNodeId(), "  Duration: %d seconds", duration);
					TODO(duration is not supported in SwitchBinary in Z-Way)
				}
				return NoError == zway_cc_switch_binary_set(GetDriver()->GetZWay(), GetNodeId(), _instance - 1, targetValue, NULL, NULL, NULL);
			}
			
//-----------------------------------------------------------------------------
// <SwitchBinary::Watcher>
// Handles Z-Way events
//-----------------------------------------------------------------------------
			void SwitchBinary::Watcher(ZWDataChangeType type, ZDataHolder data, uint8 instance)
			{
				LOG_CALL
				
				ZWay zway = GetZWay();
				
				if ((type & ~PhantomUpdate) == Updated)
				{
					if (strcmp(zdata_get_path(data), "level") == 0)
					{
						ZWBOOL level;
						LOG_ERR(zdata_get_boolean(zway_find_device_instance_cc_data(zway, GetNodeId(), instance - 1, StaticGetCommandClassId(), "level"), &level));
						
						Internal::VC::ValueBool *value = static_cast<Internal::VC::ValueBool*>(GetValue(instance, ValueID_Index_SwitchBinary::Level));
						value->OnValueRefreshed(level);
						value->Release();
					}
					TODO(Duration and target state are not reported by Z-Way)
					// static_cast<Internal::VC::ValueInt*>(GetValue(_instance, ValueID_Index_SwitchBinary::Duration))
					// static_cast<Internal::VC::ValueBool*>(GetValue(_instance, ValueID_Index_SwitchBinary::TargetState))
				}
			}
			
//-----------------------------------------------------------------------------
// <SwitchBinary::CreateVars>
// Create the values managed by this command class
//-----------------------------------------------------------------------------

			void SwitchBinary::CreateVars(uint8 const _instance)
			{
				if (Node* node = GetNodeUnsafe())
				{
					if (GetVersion() >= 2)
					{
						node->CreateValueInt(ValueID::ValueGenre_System, GetCommandClassId(), _instance, ValueID_Index_SwitchBinary::Duration, "Transition Duration", "Sec", false, false, -1, 0);
						node->CreateValueBool(ValueID::ValueGenre_System, GetCommandClassId(), _instance, ValueID_Index_SwitchBinary::TargetState, "Target State", "", true, false, true, 0);
					}
					node->CreateValueBool(ValueID::ValueGenre_User, GetCommandClassId(), _instance, ValueID_Index_SwitchBinary::Level, "Switch", "", false, false, false, 0);
					AddWatcher(_instance, "level");
				}
			}
		} // namespace CC
	} // namespace Internal
} // namespace OpenZWave
