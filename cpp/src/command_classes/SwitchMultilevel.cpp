//-----------------------------------------------------------------------------
//
//	SwitchMultilevel.cpp
//
//	Implementation of the Z-Wave COMMAND_CLASS_SWITCH_MULTILEVEL
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
#include "command_classes/SwitchMultilevel.h"
#include "command_classes/WakeUp.h"
#include "Defs.h"
#include "Msg.h"
#include "Driver.h"
#include "Node.h"
#include "platform/Log.h"

#include "value_classes/ValueBool.h"
#include "value_classes/ValueButton.h"
#include "value_classes/ValueByte.h"
#include "value_classes/ValueInt.h"

namespace OpenZWave
{
	namespace Internal
	{
		namespace CC
		{
			TODO(Rever buttons back if needed - including the handler code)
			/*
			static char const* c_switchLabelsPos[] =
			{ "Undefined", "On", "Up", "Open", "Clockwise", "Right", "Forward", "Push" };

			static char const* c_switchLabelsNeg[] =
			{ "Undefined", "Off", "Down", "Close", "Counter-Clockwise", "Left", "Reverse", "Pull" };
			*/
			
//-----------------------------------------------------------------------------
// <SwitchMultilevel::RequestState>
// Request current state from the device
//-----------------------------------------------------------------------------
			bool SwitchMultilevel::RequestState(uint32 const _requestFlags, uint8 const _instance, Driver::MsgQueue const _queue)
			{
				if (_requestFlags & RequestFlag_Dynamic)
				{
					return RequestValue(_requestFlags, ValueID_Index_SwitchMultiLevel::Level, _instance, _queue);
				}

				return false;
			}

//-----------------------------------------------------------------------------
// <SwitchMultilevel::RequestValue>
// Request current value from the device
//-----------------------------------------------------------------------------
			bool SwitchMultilevel::RequestValue(uint32 const _requestFlags, uint16 const _index, uint8 const _instance, Driver::MsgQueue const _queue)
			{
				if (_index == ValueID_Index_SwitchMultiLevel::Level)
				{
					if (m_com.GetFlagBool(COMPAT_FLAG_GETSUPPORTED))
					{
						return NoError == zway_cc_switch_multilevel_get(GetDriver()->GetZWay(), GetNodeId(), _instance, NULL, NULL, NULL);
					}
					else
					{
						Log::Write(LogLevel_Info, GetNodeId(), "SwitchMultilevelCmd_Get Not Supported on this node");
					}
				}
				return false;
			}

//-----------------------------------------------------------------------------
// <SwitchMultilevel::SetValue>
// Set the level on a device
//-----------------------------------------------------------------------------
			bool SwitchMultilevel::SetValue(Internal::VC::Value const& _value)
			{
				bool res = false;
				uint8 instance = _value.GetID().GetInstance();

				switch (_value.GetID().GetIndex())
				{
					case ValueID_Index_SwitchMultiLevel::Level:
					{
						// Level
						if (Internal::VC::ValueByte* value = static_cast<Internal::VC::ValueByte*>(GetValue(instance, ValueID_Index_SwitchMultiLevel::Level)))
						{
							res = SetLevel(instance, (static_cast<Internal::VC::ValueByte const*>(&_value))->GetValue());
							value->Release();
						}
						break;
					}
					case ValueID_Index_SwitchMultiLevel::Bright:
					{
						// Bright
						if (Internal::VC::ValueButton* button = static_cast<Internal::VC::ValueButton*>(GetValue(instance, ValueID_Index_SwitchMultiLevel::Bright)))
						{
							if (button->IsPressed())
							{
								res = StartLevelChange(instance, SwitchMultilevelDirection_Up);
							}
							else
							{
								res = StopLevelChange(instance);
							}
							button->Release();
						}
						break;
					}
					case ValueID_Index_SwitchMultiLevel::Dim:
					{
						// Dim
						if (Internal::VC::ValueButton* button = static_cast<Internal::VC::ValueButton*>(GetValue(instance, ValueID_Index_SwitchMultiLevel::Dim)))
						{
							if (button->IsPressed())
							{
								res = StartLevelChange(instance, SwitchMultilevelDirection_Down);
							}
							else
							{
								res = StopLevelChange(instance);
							}
							button->Release();
						}
						break;
					}
					case ValueID_Index_SwitchMultiLevel::IgnoreStartLevel:
					{
						if (Internal::VC::ValueBool* value = static_cast<Internal::VC::ValueBool*>(GetValue(instance, ValueID_Index_SwitchMultiLevel::IgnoreStartLevel)))
						{
							value->OnValueRefreshed((static_cast<Internal::VC::ValueBool const*>(&_value))->GetValue());
							value->Release();
						}
						res = true;
						break;
					}
					case ValueID_Index_SwitchMultiLevel::StartLevel:
					{
						if (Internal::VC::ValueByte* value = static_cast<Internal::VC::ValueByte*>(GetValue(instance, ValueID_Index_SwitchMultiLevel::StartLevel)))
						{
							value->OnValueRefreshed((static_cast<Internal::VC::ValueByte const*>(&_value))->GetValue());
							value->Release();
						}
						res = true;
						break;
					}
					case ValueID_Index_SwitchMultiLevel::Duration:
					{
						if (Internal::VC::ValueInt* value = static_cast<Internal::VC::ValueInt*>(GetValue(instance, ValueID_Index_SwitchMultiLevel::Duration)))
						{
							value->OnValueRefreshed((static_cast<Internal::VC::ValueInt const*>(&_value))->GetValue());
							value->Release();
						}
						res = true;
						break;
					}
					case ValueID_Index_SwitchMultiLevel::Step:
					{
						if (Internal::VC::ValueByte* value = static_cast<Internal::VC::ValueByte*>(GetValue(instance, ValueID_Index_SwitchMultiLevel::Step)))
						{
							value->OnValueRefreshed((static_cast<Internal::VC::ValueByte const*>(&_value))->GetValue());
							value->Release();
						}
						res = true;
						break;
					}
					case ValueID_Index_SwitchMultiLevel::Inc:
					{
						// Inc
						if (Internal::VC::ValueButton* button = static_cast<Internal::VC::ValueButton*>(GetValue(instance, ValueID_Index_SwitchMultiLevel::Inc)))
						{
							if (button->IsPressed())
							{
								res = StartLevelChange(instance, SwitchMultilevelDirection_Inc);
							}
							else
							{
								res = StopLevelChange(instance);
							}
							button->Release();
						}
						break;
					}
					case ValueID_Index_SwitchMultiLevel::Dec:
					{
						// Dec
						if (Internal::VC::ValueButton* button = static_cast<Internal::VC::ValueButton*>(GetValue(instance, ValueID_Index_SwitchMultiLevel::Dec)))
						{
							if (button->IsPressed())
							{
								res = StartLevelChange(instance, SwitchMultilevelDirection_Dec);
							}
							else
							{
								res = StopLevelChange(instance);
							}
							button->Release();
						}
						break;
					}
				}

				return res;
			}

//-----------------------------------------------------------------------------
// <SwitchMultilevel::SetValueBasic>
// Update class values based in BASIC mapping
//-----------------------------------------------------------------------------
			void SwitchMultilevel::SetValueBasic(uint8 const _instance, uint8 const _value)
			{
				if (Node* node = GetNodeUnsafe())
				{
					(void)node; TODO(do we need this node?)
					if (Internal::VC::ValueByte* value = static_cast<Internal::VC::ValueByte*>(GetValue(_instance, ValueID_Index_SwitchMultiLevel::Level)))
					{
						value->OnValueRefreshed(_value != 0);
						value->Release();
					}
				}
			}

//-----------------------------------------------------------------------------
// <SwitchMultilevel::SetLevel>
// Set a new level for the switch
//-----------------------------------------------------------------------------
			bool SwitchMultilevel::SetLevel(uint8 const _instance, uint8 const _level)
			{
				uint32 _duration = 0xFF; // Factory default
				
				if (GetVersion() >= 2)
				{
					Internal::VC::ValueInt* durationValue = static_cast<Internal::VC::ValueInt*>(GetValue(_instance, ValueID_Index_SwitchMultiLevel::Duration));
					uint32 duration = durationValue->GetValue();
					durationValue->Release();
					if (duration > 7620)
						Log::Write(LogLevel_Info, GetNodeId(), "  Duration: Device Default");
					else if (duration > 0x7F)
						Log::Write(LogLevel_Info, GetNodeId(), "  Rouding to %d Minutes (over 127 seconds)", encodeDuration(duration)-0x79);
					else 
						Log::Write(LogLevel_Info, GetNodeId(), "  Duration: %d seconds", duration);
					
					_duration = encodeDuration(duration);
				}
				
				TODO(ignore start level)
				TODO(startlevel)
				return NoError == zway_cc_switch_multilevel_set(GetDriver()->GetZWay(), GetNodeId(), _instance, _level, _duration, NULL, NULL, NULL);
			}

//-----------------------------------------------------------------------------
// <SwitchMultilevel::SwitchMultilevelCmd_StartLevelChange>
// Start the level changing
//-----------------------------------------------------------------------------
			bool SwitchMultilevel::StartLevelChange(uint8 const _instance, SwitchMultilevelDirection const _direction)
			{
				Log::Write(LogLevel_Info, GetNodeId(), "SwitchMultilevel::StartLevelChange - Starting a level change");

				if (_direction > 3)
				{
					Log::Write(LogLevel_Warning, GetNodeId(), "_direction Value was greater than range. Dropping");
					return false;
				}
				Log::Write(LogLevel_Info, GetNodeId(), "  Direction:          %d", _direction);

				uint8 ignoreStartLevel = 1;
				if (Internal::VC::ValueBool* ignoreStartLevelValue = static_cast<Internal::VC::ValueBool*>(GetValue(_instance, ValueID_Index_SwitchMultiLevel::IgnoreStartLevel)))
				{
					ignoreStartLevel = ignoreStartLevelValue->GetValue();
					ignoreStartLevelValue->Release();
				}

				uint8 startLevel = 0;
				if (Internal::VC::ValueByte* startLevelValue = static_cast<Internal::VC::ValueByte*>(GetValue(_instance, ValueID_Index_SwitchMultiLevel::StartLevel)))
				{
					startLevel = startLevelValue->GetValue();
					startLevelValue->Release();
				}
				Log::Write(LogLevel_Info, GetNodeId(), "  Start Level:        %d", startLevel);

				uint32 duration = -1;
				if (Internal::VC::ValueInt* durationValue = static_cast<Internal::VC::ValueInt*>(GetValue(_instance, ValueID_Index_SwitchMultiLevel::Duration)))
				{
					duration = durationValue->GetValue();
					durationValue->Release();
					Log::Write(LogLevel_Info, GetNodeId(), "  Duration:           %d", duration);
				}

				uint8 step = 0;
				if ((SwitchMultilevelDirection_Inc == _direction) || (SwitchMultilevelDirection_Dec == _direction))
				{
					if (Internal::VC::ValueByte* stepValue = static_cast<Internal::VC::ValueByte*>(GetValue(_instance, ValueID_Index_SwitchMultiLevel::Step)))
					{
						step = stepValue->GetValue();
						stepValue->Release();
						Log::Write(LogLevel_Info, GetNodeId(), "  Step Size:          %d", step);
					}
				}
				
				TODO(incdec)
				TODO(step)
				return NoError == zway_cc_switch_multilevel_start_level_change(GetDriver()->GetZWay(), GetNodeId(), _instance, _direction, duration, ignoreStartLevel, startLevel, 0, 0xFF, NULL, NULL, NULL);
			}

//-----------------------------------------------------------------------------
// <SwitchMultilevel::StopLevelChange>
// Stop the level changing
//-----------------------------------------------------------------------------
			bool SwitchMultilevel::StopLevelChange(uint8 const _instance)
			{
				return NoError == zway_cc_switch_multilevel_stop_level_change(GetDriver()->GetZWay(), GetNodeId(), _instance, NULL, NULL, NULL);
			}

//-----------------------------------------------------------------------------
// <SwitchMultilevel::Watcher>
// Handles Z-Way events
//-----------------------------------------------------------------------------
			void SwitchMultilevel::Watcher(const ZDataRootObject root, ZWDataChangeType type, ZDataHolder data, void *arg)
			{
				LOG_CALL
				
				ZWay zway = (ZWay)root;
				Internal::VC::ValueByte *value = (Internal::VC::ValueByte *)arg;
				
				switch(type)
				{
					case Updated:
					case PhantomUpdate:
					{
						// level
						int level;
						zdata_acquire_lock(root);
						zdata_get_integer(zway_find_device_instance_cc_data(zway, value->GetID().GetNodeId(), value->GetID().GetInstance(), StaticGetCommandClassId(), "level"), &level);
						zdata_release_lock(root);
						
						value->OnValueRefreshed(level);
						value->Release();
						
						break;
					}
					case Deleted:
					{
						delete value;
						break;
					}
					case Invalidated:
					case ChildCreated:
					case ChildEvent:
					{
						break;
					}
				}
			}

//-----------------------------------------------------------------------------
// <SwitchMultilevel::CreateVars>
// Create the values managed by this command class
//-----------------------------------------------------------------------------
			void SwitchMultilevel::CreateVars(uint8 const _instance)
			{
				if (Node* node = GetNodeUnsafe())
				{
					if (GetVersion() >= 4)
					{
						node->CreateValueByte(ValueID::ValueGenre_System, GetCommandClassId(), _instance, ValueID_Index_SwitchMultiLevel::TargetValue, "Target Value", "", true, false, 0, 0);
						TODO(Duration and target state are not reported by Z-Way)
					}
					if (GetVersion() >= 3)
					{
						node->CreateValueByte(ValueID::ValueGenre_User, GetCommandClassId(), _instance, ValueID_Index_SwitchMultiLevel::Step, "Step Size", "", false, false, 0, 0);
						node->CreateValueButton(ValueID::ValueGenre_User, GetCommandClassId(), _instance, ValueID_Index_SwitchMultiLevel::Inc, "Inc", 0);
						node->CreateValueButton(ValueID::ValueGenre_User, GetCommandClassId(), _instance, ValueID_Index_SwitchMultiLevel::Dec, "Dec", 0);
					}
					if (GetVersion() >= 2)
					{
						node->CreateValueInt(ValueID::ValueGenre_System, GetCommandClassId(), _instance, ValueID_Index_SwitchMultiLevel::Duration, "Dimming Duration", "", false, false, -1, 0);
						TODO(Duration and target state are not reported by Z-Way)
					}
					node->CreateValueByte(ValueID::ValueGenre_User, GetCommandClassId(), _instance, ValueID_Index_SwitchMultiLevel::Level, "Level", "", false, false, 0, 0);
					zdata_add_callback(zway_find_device_instance_cc_data(GetDriver()->GetZWay(), GetNodeId(), _instance, StaticGetCommandClassId(), "level"), Watcher, TRUE,
						static_cast<Internal::VC::ValueBool*>(GetValue(_instance, ValueID_Index_SwitchBinary::Level))
					);
					node->CreateValueButton(ValueID::ValueGenre_User, GetCommandClassId(), _instance, ValueID_Index_SwitchMultiLevel::Bright, "Bright", 0);
					node->CreateValueButton(ValueID::ValueGenre_User, GetCommandClassId(), _instance, ValueID_Index_SwitchMultiLevel::Dim, "Dim", 0);
					node->CreateValueBool(ValueID::ValueGenre_System, GetCommandClassId(), _instance, ValueID_Index_SwitchMultiLevel::IgnoreStartLevel, "Ignore Start Level", "", false, false, true, 0);
					node->CreateValueByte(ValueID::ValueGenre_System, GetCommandClassId(), _instance, ValueID_Index_SwitchMultiLevel::StartLevel, "Start Level", "", false, false, 0, 0);
				}
			}
		} // namespace CC
	} // namespace Internal
} // namespace OpenZWave

