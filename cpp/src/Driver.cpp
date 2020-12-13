//-----------------------------------------------------------------------------
//
//	Driver.cpp
//
//	Communicates with a Z-Wave network
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
#include "Defs.h"
#include "Driver.h"
#include "Options.h"
#include "Manager.h"
#include "Node.h"
#include "Msg.h"
#include "Notification.h"
#include "Scene.h"
#include "DNSThread.h"
#include "TimerThread.h"
#include "Http.h"
#include "ManufacturerSpecificDB.h"

#include "platform/Event.h"
#include "platform/Mutex.h"
#include "platform/Thread.h"
#include "platform/Log.h"
#include "platform/TimeStamp.h"

#include "command_classes/CommandClasses.h"
#include "command_classes/ApplicationStatus.h"
#include "command_classes/ControllerReplication.h"
#include "command_classes/Security.h"
#include "command_classes/WakeUp.h"
#include "command_classes/SwitchAll.h"
#include "command_classes/ManufacturerSpecific.h"
#include "command_classes/NoOperation.h"

#include "value_classes/ValueID.h"
#include "value_classes/Value.h"
#include "value_classes/ValueStore.h"

#include "tinyxml.h"

#include "Utils.h"
#if defined(__unix__) || (defined(__APPLE__) && defined(__MACH__))
# include <unistd.h>
#elif defined _WIN32
# include <windows.h>
#define sleep(x) Sleep(1000 * x)
#endif
#include <algorithm>
#include <iostream>
#include <sstream>
#include <iomanip>

#include "ZWayLib.h"
#include "ZLogging.h"

using namespace OpenZWave;

void print_zway_terminated(ZWay zway, void* arg)
{
	printf("%p",zway);
	printf("||| zway_terminated |||\n");
}

// Version numbering for saved configurations. Any change that will invalidate
// previously saved configurations must be accompanied by an increment to the
// version number, and a comment explaining the date of, and reason for, the change.
//
// 01: 12-31-2010 - Introduced config version numbering due to ValueID format change.
// 02: 01-12-2011 - Command class m_afterMark sense corrected, and attribute named to match.
// 03: 08-04-2011 - Changed command class instance handling for non-sequential MultiChannel endpoints.
// 04: 12-07-2019 - Changed Interview Order
// 05: 10-07-2020 - Duration ValueID's changed from Byte to Int. Invalidate Any previous caches. 
uint32 const c_configVersion = 5;

//-----------------------------------------------------------------------------
// <Driver::Driver>
// Constructor
//-----------------------------------------------------------------------------
Driver::Driver(string const& _controllerPath, ControllerInterface const& _interface) :
		m_driverThread(new Internal::Platform::Thread("driver")), m_dns(new Internal::DNSThread(this)), m_dnsThread(new Internal::Platform::Thread("dns")), m_initMutex(new Internal::Platform::Mutex()), m_exit(false), m_init(false), m_awakeNodesQueried(false), m_allNodesQueried(false), m_notifytransactions(false), m_timer(new Internal::TimerThread(this)), m_timerThread(new Internal::Platform::Thread("timer")), m_controllerInterfaceType(_interface), m_controllerPath(_controllerPath),
		m_homeId(0), m_libraryVersion(""), m_libraryTypeName(""), m_libraryType(0), m_manufacturerId(0), m_productType(0), m_productId(0), m_initVersion(0), m_initCaps(0), m_controllerCaps(0), m_Controller_nodeId(0), m_nodeMutex(new Internal::Platform::Mutex()), m_controllerReplication( NULL), m_transmitOptions( TRANSMIT_OPTION_ACK | TRANSMIT_OPTION_AUTO_ROUTE | TRANSMIT_OPTION_EXPLORE),
		m_pollInterval(0), m_bIntervalBetweenPolls(false),				// if set to true (via SetPollInterval), the pollInterval will be interspersed between each poll (so a much smaller m_pollInterval like 100, 500, or 1,000 may be appropriate)
		m_currentControllerCommand( NULL), m_notificationsEvent(new Internal::Platform::Event()),
		AuthKey(0), EncryptKey(0), m_nonceReportSent(0), m_nonceReportSentAttempt(0), m_queueMsgEvent(new Internal::Platform::Event()), m_eventMutex(new Internal::Platform::Mutex())
{
	LOG_CALL
	// set a timestamp to indicate when this driver started
	Internal::Platform::TimeStamp m_startTime;

	// Clear the nodes array
	memset(m_nodes, 0, sizeof(Node*) * 256);

// 	// Initialize the Network Keys

// 	Options::Get()->GetOptionAsBool("NotifyTransactions", &m_notifytransactions);
// 	Options::Get()->GetOptionAsInt("PollInterval", &m_pollInterval);
// 	Options::Get()->GetOptionAsBool("IntervalBetweenPolls", &m_bIntervalBetweenPolls);

	TODO(remove those funcitons from the project If public - make dummies)
// 	m_mfs = Internal::ManufacturerSpecificDB::Create();
// 	CheckMFSConfigRevision();

	// OZWay begin
	TODO(exceptions)
	ZWError r;
	// printf("%p\n", Manager::Get()->m_logger);
	r = zway_init(&zway, ZSTR(_controllerPath.c_str()), NULL, NULL, NULL, NULL, Manager::Get()->m_logger);
	if (r != NoError)
	{
		printf(">> Adding driver error: %s\n", zstrerror(r));
		return;
	}
	r = zway_start(zway, print_zway_terminated, NULL);
	if (r != NoError)
	{
		printf(">> Driver starting error: %s\n", zstrerror(r));
		return;
	}
	r = zway_discover(zway);
	if (r != NoError)
	{
		printf(">> Driver discovering error: %s\n", zstrerror(r));
		return;
	}

	// Getting HomeID
	zdata_acquire_lock(ZDataRoot(zway));
	TODO(LOG_ERR)
	{
		int t;
		zdata_get_integer(zway_find_controller_data(zway, "nodeId"), &t);
		m_Controller_nodeId = (uint8)t;
	}
	zdata_get_integer(zway_find_controller_data(zway, "homeId"), (int *)&m_homeId);
	zdata_release_lock(ZDataRoot(zway));

	m_initVersion = 0; TODO(set this field as _data[2] of SerialAPIInit reply)
	m_initCaps = 0; TODO(set this field as _data[3] of SerialAPIInit reply)
	//OZWay end
	
}

//-----------------------------------------------------------------------------
// <Driver::Driver>
// Destructor
//-----------------------------------------------------------------------------
Driver::~Driver()
{
	LOG_CALL

	/* Signal that we are going away... so at least Apps know... */
	Notification* notification = new Notification(Notification::Type_DriverRemoved);
	notification->SetHomeAndNodeIds(m_homeId, 0);
	QueueNotification(notification);
	NotifyWatchers();

	// append final driver stats output to the log file
	LogDriverStatistics();
	// Save the driver config before deleting anything else
	bool save;
	if (Options::Get()->GetOptionAsBool("SaveConfiguration", &save))
	{
		if (save)
		{
			WriteCache();
			Internal::Scene::WriteXML("zwscene.xml");
		}
	}

	// The order of the statements below has been achieved by mitigating freed memory
	//references using a memory allocator checker. Do not rearrange unless you are
	//certain memory won't be referenced out of order. --Greg Satz, April 2010
	m_initMutex->Lock();
	m_exit = true;
	m_initMutex->Unlock();

	m_dnsThread->Stop();
	m_dnsThread->Release();

	m_driverThread->Stop();
	m_driverThread->Release();

	m_timerThread->Stop();
	m_timerThread->Release();

	m_initMutex->Release();

	// Clear the node data
	{
		Internal::LockGuard LG(m_nodeMutex);
		for (int i = 0; i < 256; ++i)
		{
			if (GetNodeUnsafe(i))
			{
				delete m_nodes[i];
				m_nodes[i] = NULL;
				Notification* notification = new Notification(Notification::Type_NodeRemoved);
				notification->SetHomeAndNodeIds(m_homeId, i);
				QueueNotification(notification);
			}
		}
	}

	/* Doing our Notification Call back here in the destructor is just asking for trouble
	 * as there is a good chance that the application will do some sort of GetDriver() supported
	 * method on the Manager Class, which by this time, most of the OZW Classes associated with the
	 * Driver class is 99% destructed. (mainly nodes, which cascade to CC, which cascade to ValueID
	 * classes etc). We might need a flag around the Manager::GetDriver() class that stops applications
	 * from getting a half destructed Driver Reference, but still retain a Internal GetDriver() method
	 * that can return half destructed Driver references for internal classes (as per Greg's note above)
	 */
	bool notify;
	if (Options::Get()->GetOptionAsBool("NotifyOnDriverUnload", &notify))
	{
		if (notify)
		{
			NotifyWatchers();
		}
	}

	list<Notification*>::iterator nit = m_notifications.begin();
	while (nit != m_notifications.end())
	{
		Notification* notification = m_notifications.front();
		m_notifications.pop_front();
		delete notification;
		nit = m_notifications.begin();
	}

	if (m_controllerReplication)
		delete m_controllerReplication;

	m_notificationsEvent->Release();
	m_nodeMutex->Release();
	m_queueMsgEvent->Release();
	m_eventMutex->Release();
	delete this->AuthKey;
	delete this->EncryptKey;
	delete this->m_httpClient;
	delete this->m_timer;
	delete this->m_dns;
	
	// OZWay begin
	ZWError r;
	r = zway_stop(zway);
	if (r != NoError)
	{
		printf(">> Driver stopping error: %s\n", zstrerror(r));
	}
	zway_terminate(&zway);
	// OZWay end
}

//-----------------------------------------------------------------------------
// <Driver::Start>
// Start the driver thread
//-----------------------------------------------------------------------------
void Driver::Start()
{
	LOG_CALL
	// // Start the thread that will handle communications with the Z-Wave network
	// m_driverThread->Start(Driver::DriverThreadEntryPoint, this);
	// m_dnsThread->Start(Internal::DNSThread::DNSThreadEntryPoint, m_dns);
	// m_timerThread->Start(Internal::TimerThread::TimerThreadEntryPoint, m_timer);

	Manager::Get()->SetDriverReady(this, true);
	ReadCache();

	//OZWay begin
	zdata_acquire_lock(ZDataRoot(zway));
	for (uint8 nodeId = 1; nodeId <= 232; ++nodeId)
	{
		if (zway_find_device_data(zway, nodeId, "") != NULL)
		{
			Internal::LockGuard LG(m_nodeMutex);
			Node* node = GetNode(nodeId);
			if (node)
			{
				Log::Write(LogLevel_Info, "    Node %.3d - Known", nodeId);
				if (!m_init)
				{
					// The node was read in from the config, so we
					// only need to get its current state
					node->SetQueryStage(Node::QueryStage_CacheLoad);
				}

			}
			else
			{
				// This node is new
				Log::Write(LogLevel_Info, "    Node %.3d - New", nodeId);
				Notification* notification = new Notification(Notification::Type_NodeNew);
				notification->SetHomeAndNodeIds(m_homeId, nodeId);
				QueueNotification(notification);

				// Create the node and request its info
				InitNode(nodeId);
				node = GetNode(nodeId);
			}
			m_nodes[nodeId] = node;
		}
	}
	zdata_release_lock(ZDataRoot(zway));
	//OZWay end
}

//-----------------------------------------------------------------------------
// <Driver::DriverThreadEntryPoint>
// Entry point of the thread for creating and managing the worker threads
//-----------------------------------------------------------------------------
void Driver::DriverThreadEntryPoint(Internal::Platform::Event* _exitEvent, void* _context)
{
	LOG_CALL
	Driver* driver = (Driver*) _context;
	if (driver)
	{
		driver->DriverThreadProc(_exitEvent);
	}
}

//-----------------------------------------------------------------------------
// <Driver::DriverThreadProc>
// Create and manage the worker threads
//-----------------------------------------------------------------------------
void Driver::DriverThreadProc(Internal::Platform::Event* _exitEvent)
{
	LOG_CALL

	uint32 attempts = 0;
	while (true)
	{
		if (Init(attempts)) break;

		++attempts;

		uint32 maxAttempts = 0;
		Options::Get()->GetOptionAsInt("DriverMaxAttempts", (int32 *) &maxAttempts);
		if (maxAttempts && (attempts >= maxAttempts))
		{
			Manager::Get()->Manager::SetDriverReady(this, false);
			NotifyWatchers();
			break;
		}

		if (attempts < 25)
		{
			// Retry every 5 seconds for the first two minutes
			if (Internal::Platform::Wait::Single(_exitEvent, 5000) == 0)
			{
				// Exit signalled.
				m_initMutex->Lock();
				m_exit = true;
				m_initMutex->Unlock();
				return;
			}
		}
		else
		{
			// Retry every 30 seconds after that
			if (Internal::Platform::Wait::Single(_exitEvent, 30000) == 0)
			{
				// Exit signalled.
				m_initMutex->Lock();
				m_exit = true;
				m_initMutex->Unlock();
				return;
			}
		}
	}
}

//-----------------------------------------------------------------------------
// <Driver::Init>
// Initialize the controller
//-----------------------------------------------------------------------------
bool Driver::Init(uint32 _attempts)
{
	LOG_CALL
	m_initMutex->Lock();

	if (m_exit)
	{
		m_initMutex->Unlock();
		return false;
	}

	m_initMutex->Unlock();

	// Init successful
	return true;
}

//-----------------------------------------------------------------------------
// <Driver::RemoveQueues>
// Clean up any messages to a node
//-----------------------------------------------------------------------------
void Driver::RemoveQueues(uint8 const _nodeId)
{
	LOG_CALL
	
	TODO(To implement - is it needed?)
}

//-----------------------------------------------------------------------------
//	Configuration
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// <Driver::ReadCache>
// Read our configuration from an XML document
//-----------------------------------------------------------------------------
bool Driver::ReadCache()
{
	LOG_CALL
	char str[32];
	int32 intVal;

	// Load the XML document that contains the driver configuration
	string userPath;
	Options::Get()->GetOptionAsString("UserPath", &userPath);

	snprintf(str, sizeof(str), "ozwcache_0x%08x.xml", m_homeId);
	string filename = userPath + string(str);

	TiXmlDocument doc;
	if (!doc.LoadFile(filename.c_str(), TIXML_ENCODING_UTF8))
	{
		return false;
	}
	doc.SetUserData((void *) filename.c_str());
	TiXmlElement const* driverElement = doc.RootElement();

	char const *xmlns = driverElement->Attribute("xmlns");
	if (strcmp(xmlns, "https://github.com/OpenZWave/open-zwave"))
	{
		Log::Write(LogLevel_Warning, "Invalid XML Namespace. Ignoring %s", filename.c_str());
		return false;
	}

	// Version
	if (TIXML_SUCCESS != driverElement->QueryIntAttribute("version", &intVal) || (uint32) intVal != c_configVersion)
	{
		Log::Write(LogLevel_Warning, "WARNING: Driver::ReadCache - %s is from an older version of OpenZWave and cannot be loaded.", filename.c_str());
		return false;
	}

	// Capabilities
	if (TIXML_SUCCESS == driverElement->QueryIntAttribute("revision", &intVal))
	{
		m_mfs->setLatestRevision(intVal);
	}

	// Home ID
	char const* homeIdStr = driverElement->Attribute("home_id");
	if (homeIdStr)
	{
		char* p;
		uint32 homeId = (uint32) strtoul(homeIdStr, &p, 0);

		if (homeId != m_homeId)
		{
			Log::Write(LogLevel_Warning, "WARNING: Driver::ReadCache - Home ID in file %s is incorrect", filename.c_str());
			return false;
		}
	}
	else
	{
		Log::Write(LogLevel_Warning, "WARNING: Driver::ReadCache - Home ID is missing from file %s", filename.c_str());
		return false;
	}

	// Node ID
	if (TIXML_SUCCESS == driverElement->QueryIntAttribute("node_id", &intVal))
	{
		if ((uint8) intVal != m_Controller_nodeId)
		{
			Log::Write(LogLevel_Warning, "WARNING: Driver::ReadCache - Controller Node ID in file %s is incorrect", filename.c_str());
			return false;
		}
	}
	else
	{
		Log::Write(LogLevel_Warning, "WARNING: Driver::ReadCache - Node ID is missing from file %s", filename.c_str());
		return false;
	}

	// Capabilities
	if (TIXML_SUCCESS == driverElement->QueryIntAttribute("api_capabilities", &intVal))
	{
		m_initCaps = (uint8) intVal;
	}

	if (TIXML_SUCCESS == driverElement->QueryIntAttribute("controller_capabilities", &intVal))
	{
		m_controllerCaps = (uint8) intVal;
	}

	// Poll Interval
	if (TIXML_SUCCESS == driverElement->QueryIntAttribute("poll_interval", &intVal))
	{
		m_pollInterval = intVal;
	}

	// Poll Interval--between polls or period for polling the entire pollList?
	char const* cstr = driverElement->Attribute("poll_interval_between");
	if (cstr)
	{
		m_bIntervalBetweenPolls = !strcmp(cstr, "true");
	}

	// Read the nodes
	Internal::LockGuard LG(m_nodeMutex);
	TiXmlElement const* nodeElement = driverElement->FirstChildElement();
	while (nodeElement)
	{
		char const* str = nodeElement->Value();
		if (str && !strcmp(str, "Node"))
		{
			// Get the node Id from the XML
			if (TIXML_SUCCESS == nodeElement->QueryIntAttribute("id", &intVal))
			{
				uint8 nodeId = (uint8) intVal;
				Node* node = new Node(m_homeId, nodeId);
				m_nodes[nodeId] = node;

				Notification* notification = new Notification(Notification::Type_NodeAdded);
				notification->SetHomeAndNodeIds(m_homeId, nodeId);
				QueueNotification(notification);

				// Read the rest of the node configuration from the XML
				node->ReadXML(nodeElement);
			}
		}

		nodeElement = nodeElement->NextSiblingElement();
	}

	LG.Unlock();

	// restore the previous state (for now, polling) for the nodes/values just retrieved
	for (int i = 0; i < 256; i++)
	{
		if (m_nodes[i] != NULL)
		{
			Internal::VC::ValueStore* vs = m_nodes[i]->m_values;
			for (Internal::VC::ValueStore::Iterator it = vs->Begin(); it != vs->End(); ++it)
			{
				Internal::VC::Value* value = it->second;
				if (value->m_pollIntensity != 0)
					EnablePoll(value->GetID(), value->m_pollIntensity);
			}
		}
	}

	return true;
}

//-----------------------------------------------------------------------------
// <Driver::WriteCache>
// Write ourselves to an XML document
//-----------------------------------------------------------------------------
void Driver::WriteCache()
{
	LOG_CALL
	char str[32];

	if (!m_homeId)
	{
		Log::Write(LogLevel_Warning, "WARNING: Tried to write driver config with no home ID set");
		return;
	}
	if (m_exit) {
		Log::Write(LogLevel_Info, "Skipping Cache Save as we are shutting down");
		return;
	}

	Log::Write(LogLevel_Info, "Saving Cache");
	// Create a new XML document to contain the driver configuration
	TiXmlDocument doc;
	TiXmlDeclaration* decl = new TiXmlDeclaration("1.0", "utf-8", "");
	TiXmlElement* driverElement = new TiXmlElement("Driver");
	doc.LinkEndChild(decl);
	doc.LinkEndChild(driverElement);

	driverElement->SetAttribute("xmlns", "https://github.com/OpenZWave/open-zwave");

	snprintf(str, sizeof(str), "%d", c_configVersion);
	driverElement->SetAttribute("version", str);

	snprintf(str, sizeof(str), "%d", GetManufacturerSpecificDB()->getRevision());
	driverElement->SetAttribute("revision", str);

	snprintf(str, sizeof(str), "0x%.8x", m_homeId);
	driverElement->SetAttribute("home_id", str);

	snprintf(str, sizeof(str), "%d", m_Controller_nodeId);
	driverElement->SetAttribute("node_id", str);

	snprintf(str, sizeof(str), "%d", m_initCaps);
	driverElement->SetAttribute("api_capabilities", str);

	snprintf(str, sizeof(str), "%d", m_controllerCaps);
	driverElement->SetAttribute("controller_capabilities", str);

	snprintf(str, sizeof(str), "%d", m_pollInterval);
	driverElement->SetAttribute("poll_interval", str);

	snprintf(str, sizeof(str), "%s", m_bIntervalBetweenPolls ? "true" : "false");
	driverElement->SetAttribute("poll_interval_between", str);

	{
		Internal::LockGuard LG(m_nodeMutex);

		for (int i = 0; i < 256; ++i)
		{
			if (m_nodes[i])
			{
				if (m_nodes[i]->GetCurrentQueryStage() >= Node::QueryStage_CacheLoad)
				{
					m_nodes[i]->WriteXML(driverElement);
					Log::Write(LogLevel_Info, i, "Cache Save for Node %d as its QueryStage_CacheLoad", i);
				}
				else
				{
					Log::Write(LogLevel_Info, i, "Skipping Cache Save for Node %d as its not past QueryStage_CacheLoad", i);
				}
			}
		}
	}
	string userPath;
	Options::Get()->GetOptionAsString("UserPath", &userPath);

	snprintf(str, sizeof(str), "ozwcache_0x%08x.xml", m_homeId);
	string filename = userPath + string(str);

	doc.SaveFile(filename.c_str());
}

//-----------------------------------------------------------------------------
//	Controller
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// <Driver::GetNodeUnsafe>
// Returns a pointer to the requested node without locking.
// Only to be used by main thread code.
//-----------------------------------------------------------------------------
Node* Driver::GetNodeUnsafe(uint8 _nodeId)
{
	LOG_CALL
	if (Node* node = m_nodes[_nodeId])
	{
		return node;
	}
	return NULL;
}

//-----------------------------------------------------------------------------
// <Driver::GetNode>
// Locks the nodes and returns a pointer to the requested one
//-----------------------------------------------------------------------------
Node* Driver::GetNode(uint8 _nodeId)
{
	LOG_CALL
	if (m_nodeMutex->IsSignalled())
	{
		Log::Write(LogLevel_Error, _nodeId, "Driver Thread is Not Locked during Call to GetNode");
		return NULL;
	}
	if (Node* node = m_nodes[_nodeId])
	{
		return node;
	}

	return NULL;
}

//-----------------------------------------------------------------------------
//	Sending Z-Wave messages
//-----------------------------------------------------------------------------

// <Driver::CheckCompletedNodeQueries>
// Identify controller (as opposed to node) commands...especially blocking ones
//-----------------------------------------------------------------------------
void Driver::CheckCompletedNodeQueries()
{
TODO(нужно?)
			if (1) // deadFound)
			{
				// only dead nodes left to query
				Log::Write(LogLevel_Info, "         Node query processing complete except for dead nodes.");
				Notification* notification = new Notification(Notification::Type_AllNodesQueriedSomeDead);
				notification->SetHomeAndNodeIds(m_homeId, 0xff);
				QueueNotification(notification);
			}
			else
			{
				// no sleeping nodes, no dead nodes and no more nodes in the queue, so...All done
				Log::Write(LogLevel_Info, "         Node query processing complete.");
				Notification* notification = new Notification(Notification::Type_AllNodesQueried);
				notification->SetHomeAndNodeIds(m_homeId, 0xff);
				QueueNotification(notification);
			}
			m_awakeNodesQueried = true;
			m_allNodesQueried = true;

			if (!m_awakeNodesQueried)
			{
				// only sleeping nodes remain, so signal awake nodes queried complete
				Log::Write(LogLevel_Info, "         Node query processing complete except for sleeping nodes.");
				Notification* notification = new Notification(Notification::Type_AwakeNodesQueried);
				notification->SetHomeAndNodeIds(m_homeId, 0xff);
				QueueNotification(notification);
				m_awakeNodesQueried = true;
			}
	WriteCache();
}

//-----------------------------------------------------------------------------
//	Polling Z-Wave devices
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// <Driver::EnablePoll>
// Enable polling of a value
//-----------------------------------------------------------------------------
bool Driver::EnablePoll(ValueID const &_valueId, uint8 const _intensity)
{
	LOG_CALL

	// confirm that this node exists
	uint8 nodeId = _valueId.GetNodeId();
	Internal::LockGuard LG(m_nodeMutex);
	Node* node = GetNode(nodeId);
	if (node != NULL)
	{
		// confirm that this value is in the node's value store
		if (Internal::VC::Value* value = node->GetValue(_valueId))
		{
			// update the value's pollIntensity
			value->SetPollIntensity(_intensity);

			// send notification to indicate polling is enabled
			Notification* notification = new Notification(Notification::Type_PollingEnabled);
			notification->SetHomeAndNodeIds(m_homeId, _valueId.GetNodeId());
			notification->SetValueId(_valueId);
			QueueNotification(notification);
			Log::Write(LogLevel_Info, nodeId, "EnablePoll for HomeID 0x%.8x, value(cc=0x%02x,in=0x%02x,id=0x%02x)--poll list has %d items", _valueId.GetHomeId(), _valueId.GetCommandClassId(), _valueId.GetIndex(), _valueId.GetInstance(), 0);
			WriteCache();
			return true;
		}


		Log::Write(LogLevel_Info, nodeId, "EnablePoll failed - value not found for node %d", nodeId);
		return false;
	}

	Log::Write(LogLevel_Info, "EnablePoll failed - node %d not found", nodeId);
	return false;
}

//-----------------------------------------------------------------------------
// <Driver::DisablePoll>
// Disable polling of a node
//-----------------------------------------------------------------------------
bool Driver::DisablePoll(ValueID const &_valueId)
{
	LOG_CALL

	// confirm that this node exists
	uint8 nodeId = _valueId.GetNodeId();
	Internal::LockGuard LG(m_nodeMutex);
	Node* node = GetNode(nodeId);
	if (node != NULL)
	{
				// send notification to indicate polling is disabled
				Notification* notification = new Notification(Notification::Type_PollingDisabled);
				notification->SetHomeAndNodeIds(m_homeId, _valueId.GetNodeId());
				notification->SetValueId(_valueId);
				QueueNotification(notification);
				Log::Write(LogLevel_Info, nodeId, "DisablePoll for HomeID 0x%.8x, value(cc=0x%02x,in=0x%02x,id=0x%02x)--poll list has %d items", _valueId.GetHomeId(), _valueId.GetCommandClassId(), _valueId.GetIndex(), _valueId.GetInstance(), 0);
				WriteCache();
	}

	Log::Write(LogLevel_Info, "DisablePoll failed - node %d not found", nodeId);
	return false;
}

//-----------------------------------------------------------------------------
// <Driver::SendMsg>
// Queue a message to be sent to the Z-Wave PC Interface
//-----------------------------------------------------------------------------
void Driver::SendMsg(Internal::Msg* _msg, MsgQueue const _queue)
{
	Log::Write(LogLevel_Error, "Driver::SendMsg", _msg, _queue);
}

//-----------------------------------------------------------------------------
// <Driver::isPolled>
// Check polling status of a value
//-----------------------------------------------------------------------------
bool Driver::isPolled(ValueID const &_valueId)
{
	LOG_CALL
	bool bPolled;


	Internal::VC::Value* value = GetValue(_valueId);
	if (value && value->GetPollIntensity() != 0)
	{
		bPolled = true;
	}
	else
	{
		bPolled = false;
	}

	if (value)
		value->Release();

	/*
	 * This code is retained for the moment as a belt-and-suspenders test to confirm that
	 * the pollIntensity member of each value and the pollList contents do not get out
	 * of sync.
	 */
	// confirm that this node exists
	uint8 nodeId = _valueId.GetNodeId();
	Internal::LockGuard LG(m_nodeMutex);
	Node* node = GetNode(nodeId);
	if (node != NULL)
	{
		return bPolled;
	}

	Log::Write(LogLevel_Info, "isPolled failed - node %d not found (the value reported that it is%s polled)", nodeId, bPolled ? "" : " not");
	return false;
}

//-----------------------------------------------------------------------------
// <Driver::SetPollIntensity>
// Set the intensity with which this value is polled
//-----------------------------------------------------------------------------
void Driver::SetPollIntensity(ValueID const &_valueId, uint8 const _intensity)
{
	LOG_CALL
	
	TODO(Do we need this?)

	Internal::VC::Value* value = GetValue(_valueId);
	if (!value)
		return;
	value->SetPollIntensity(_intensity);

	value->Release();
	WriteCache();
}

//-----------------------------------------------------------------------------
//	Retrieving Node information
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// <Driver::InitAllNodes>
// Delete all nodes and fetch new node data from the Z-Wave network
//-----------------------------------------------------------------------------
void Driver::InitAllNodes()
{
	LOG_CALL
	// Delete all the node data
	{
		Internal::LockGuard LG(m_nodeMutex);
		for (int i = 0; i < 256; ++i)
		{
			if (m_nodes[i])
			{
				delete m_nodes[i];
				m_nodes[i] = NULL;
			}
		}
	}
	// Kick off the Initilization Sequence again
	SendMsg(new Internal::Msg("FUNC_ID_ZW_GET_VERSION", 0xff, REQUEST, FUNC_ID_ZW_GET_VERSION, false), Driver::MsgQueue_Command);
}

//-----------------------------------------------------------------------------
// <Driver::InitNode>
// Queue a node to be interrogated for its setup details
//-----------------------------------------------------------------------------
void Driver::InitNode(uint8 const _nodeId, bool newNode, bool secure, uint8 const *_protocolInfo, uint8 const _length)
{
	// Delete any existing node and replace it with a new one
	{
		Internal::LockGuard LG(m_nodeMutex);
		if (m_nodes[_nodeId])
		{
			// Remove the original node
			delete m_nodes[_nodeId];
			m_nodes[_nodeId] = NULL;
			WriteCache();
			Notification* notification = new Notification(Notification::Type_NodeRemoved);
			notification->SetHomeAndNodeIds(m_homeId, _nodeId);
			QueueNotification(notification);
		}

		// Add the new node
		m_nodes[_nodeId] = new Node(m_homeId, _nodeId);
		if (newNode == true)
			static_cast<Node *>(m_nodes[_nodeId])->SetAddingNode();
	}

	Notification* notification = new Notification(Notification::Type_NodeAdded);
	notification->SetHomeAndNodeIds(m_homeId, _nodeId);
	QueueNotification(notification);

	if (_length == 0)
	{
		// Request the node info
		TODO(Fix in future) // m_nodes[_nodeId]->SetQueryStage(Node::QueryStage_ProtocolInfo);
	}
	else
	{
		if (isNetworkKeySet())
			m_nodes[_nodeId]->SetSecured(secure);
		else
			Log::Write(LogLevel_Info, _nodeId, "Network Key Not Set - Secure Option is %s", secure ? "required" : "not required");
		m_nodes[_nodeId]->SetProtocolInfo(_protocolInfo, _length);
	}
	Log::Write(LogLevel_Info, _nodeId, "Initializing Node. New Node: %s (%s)", static_cast<Node *>(m_nodes[_nodeId])->IsAddingNode() ? "true" : "false", newNode ? "true" : "false");
}

//-----------------------------------------------------------------------------
// <Driver::IsNodeListeningDevice>
// Get whether the node is a listening device that does not go to sleep
//-----------------------------------------------------------------------------
bool Driver::IsNodeListeningDevice(uint8 const _nodeId)
{
	bool res = false;
	Internal::LockGuard LG(m_nodeMutex);
	if (Node* node = GetNode(_nodeId))
	{
		res = node->IsListeningDevice();
	}

	return res;
}

//-----------------------------------------------------------------------------
// <Driver::IsNodeFrequentListeningDevice>
// Get whether the node is a listening device that does not go to sleep
//-----------------------------------------------------------------------------
bool Driver::IsNodeFrequentListeningDevice(uint8 const _nodeId)
{
	bool res = false;
	Internal::LockGuard LG(m_nodeMutex);
	if (Node* node = GetNode(_nodeId))
	{
		res = node->IsFrequentListeningDevice();
	}

	return res;
}

//-----------------------------------------------------------------------------
// <Driver::IsNodeBeamingDevice>
// Get whether the node is a beam capable device.
//-----------------------------------------------------------------------------
bool Driver::IsNodeBeamingDevice(uint8 const _nodeId)
{
	bool res = false;
	Internal::LockGuard LG(m_nodeMutex);
	if (Node* node = GetNode(_nodeId))
	{
		res = node->IsBeamingDevice();
	}

	return res;
}

//-----------------------------------------------------------------------------
// <Driver::IsNodeRoutingDevice>
// Get whether the node is a routing device that passes messages to other nodes
//-----------------------------------------------------------------------------
bool Driver::IsNodeRoutingDevice(uint8 const _nodeId)
{
	bool res = false;
	Internal::LockGuard LG(m_nodeMutex);
	if (Node* node = GetNode(_nodeId))
	{
		res = node->IsRoutingDevice();
	}

	return res;
}

//-----------------------------------------------------------------------------
// <Driver::IsNodeSecurityDevice>
// Get the security attribute for a node
//-----------------------------------------------------------------------------
bool Driver::IsNodeSecurityDevice(uint8 const _nodeId)
{
	bool security = false;
	Internal::LockGuard LG(m_nodeMutex);
	if (Node* node = GetNode(_nodeId))
	{
		security = node->IsSecurityDevice();
	}

	return security;
}

//-----------------------------------------------------------------------------
// <Driver::GetNodeMaxBaudRate>
// Get the maximum baud rate of a node's communications
//-----------------------------------------------------------------------------
uint32 Driver::GetNodeMaxBaudRate(uint8 const _nodeId)
{
	uint32 baud = 0;
	Internal::LockGuard LG(m_nodeMutex);
	if (Node* node = GetNode(_nodeId))
	{
		baud = node->GetMaxBaudRate();
	}

	return baud;
}

//-----------------------------------------------------------------------------
// <Driver::GetNodeVersion>
// Get the version number of a node
//-----------------------------------------------------------------------------
uint8 Driver::GetNodeVersion(uint8 const _nodeId)
{
	uint8 version = 0;
	Internal::LockGuard LG(m_nodeMutex);
	if (Node* node = GetNode(_nodeId))
	{
		version = node->GetVersion();
	}

	return version;
}

//-----------------------------------------------------------------------------
// <Driver::GetNodeSecurity>
// Get the security byte of a node
//-----------------------------------------------------------------------------
uint8 Driver::GetNodeSecurity(uint8 const _nodeId)
{
	uint8 security = 0;
	Internal::LockGuard LG(m_nodeMutex);
	if (Node* node = GetNode(_nodeId))
	{
		security = node->GetSecurity();
	}

	return security;
}

//-----------------------------------------------------------------------------
// <Driver::GetNodeBasic>
// Get the basic type of a node
//-----------------------------------------------------------------------------
uint8 Driver::GetNodeBasic(uint8 const _nodeId)
{
	uint8 basic = 0;
	Internal::LockGuard LG(m_nodeMutex);
	if (Node* node = GetNode(_nodeId))
	{
		basic = node->GetBasic();
	}

	return basic;
}

//-----------------------------------------------------------------------------
// <Driver::GetNodeBasic>
// Get the basic type of a node
//-----------------------------------------------------------------------------
string Driver::GetNodeBasicString(uint8 const _nodeId)
{
	Internal::LockGuard LG(m_nodeMutex);
	if (Node* node = GetNode(_nodeId))
	{
		return node->GetBasicString();
	}

	return "Unknown";
}




//-----------------------------------------------------------------------------
// <Driver::GetNodeGeneric>
// Get the generic type of a node
//-----------------------------------------------------------------------------
uint8 Driver::GetNodeGeneric(uint8 const _nodeId, uint8 const _instance)
{
	uint8 genericType = 0;
	Internal::LockGuard LG(m_nodeMutex);
	if (Node* node = GetNode(_nodeId))
	{
		genericType = node->GetGeneric(_instance);
	}

	return genericType;
}

//-----------------------------------------------------------------------------
// <Driver::GetNodeGeneric>
// Get the generic type of a node
//-----------------------------------------------------------------------------
string Driver::GetNodeGenericString(uint8 const _nodeId, uint8 const _instance)
{
	Internal::LockGuard LG(m_nodeMutex);
	if (Node* node = GetNode(_nodeId))
	{
		return node->GetGenericString(_instance);
	}

	return "Unknown";
}


//-----------------------------------------------------------------------------
// <Driver::GetNodeSpecific>
// Get the specific type of a node
//-----------------------------------------------------------------------------
uint8 Driver::GetNodeSpecific(uint8 const _nodeId, uint8 const _instance)
{
	uint8 specific = 0;
	Internal::LockGuard LG(m_nodeMutex);
	if (Node* node = GetNode(_nodeId))
	{
		specific = node->GetSpecific(_instance);
	}

	return specific;
}

//-----------------------------------------------------------------------------
// <Driver::GetNodeSpecific>
// Get the specific type of a node
//-----------------------------------------------------------------------------
string Driver::GetNodeSpecificString(uint8 const _nodeId, uint8 const _instance)
{
	Internal::LockGuard LG(m_nodeMutex);
	if (Node* node = GetNode(_nodeId))
	{
		return node->GetSpecificString(_instance);
	}

	return "Unknown";
}

//-----------------------------------------------------------------------------
// <Driver::GetNodeType>
// Get the basic/generic/specific type of the specified node
// Returns a copy of the string rather than a const ref for thread safety
//-----------------------------------------------------------------------------
string Driver::GetNodeType(uint8 const _nodeId)
{
	Internal::LockGuard LG(m_nodeMutex);
	if (Node* node = GetNode(_nodeId))
	{
		return node->GetType();
	}

	return "Unknown";
}

bool Driver::IsNodeZWavePlus(uint8 const _nodeId)
{
	Internal::LockGuard LG(m_nodeMutex);
	if (Node* node = GetNode(_nodeId))
	{
		return node->IsNodeZWavePlus();
	}
	return false;
}

//-----------------------------------------------------------------------------
// <Driver::GetNodeNeighbors>
// Gets the neighbors for a node
//-----------------------------------------------------------------------------
uint32 Driver::GetNodeNeighbors(uint8 const _nodeId, uint8** o_neighbors)
{
	uint32 numNeighbors = 0;
	Internal::LockGuard LG(m_nodeMutex);
	if (Node* node = GetNode(_nodeId))
	{
		numNeighbors = node->GetNeighbors(o_neighbors);
	}

	return numNeighbors;
}

//-----------------------------------------------------------------------------
// <Driver::GetNodeManufacturerName>
// Get the manufacturer name for the node with the specified ID
// Returns a copy of the string rather than a const ref for thread safety
//-----------------------------------------------------------------------------
string Driver::GetNodeManufacturerName(uint8 const _nodeId)
{
	Internal::LockGuard LG(m_nodeMutex);
	if (Node* node = GetNode(_nodeId))
	{
		return node->GetManufacturerName();
	}

	return "";
}

//-----------------------------------------------------------------------------
// <Driver::GetNodeProductName>
// Get the product name for the node with the specified ID
// Returns a copy of the string rather than a const ref for thread safety
//-----------------------------------------------------------------------------
string Driver::GetNodeProductName(uint8 const _nodeId)
{
	Internal::LockGuard LG(m_nodeMutex);
	if (Node* node = GetNode(_nodeId))
	{
		return node->GetProductName();
	}

	return "";
}

//-----------------------------------------------------------------------------
// <Driver::GetNodeName>
// Get the user-editable name for the node with the specified ID
// Returns a copy of the string rather than a const ref for thread safety
//-----------------------------------------------------------------------------
string Driver::GetNodeName(uint8 const _nodeId)
{
	Internal::LockGuard LG(m_nodeMutex);
	if (Node* node = GetNode(_nodeId))
	{
		return node->GetNodeName();
	}

	return "";
}

//-----------------------------------------------------------------------------
// <Driver::GetNodeLocation>
// Get the user-editable string for location of the specified node
// Returns a copy of the string rather than a const ref for thread safety
//-----------------------------------------------------------------------------
string Driver::GetNodeLocation(uint8 const _nodeId)
{
	Internal::LockGuard LG(m_nodeMutex);
	if (Node* node = GetNode(_nodeId))
	{
		return node->GetLocation();
	}

	return "";
}

//-----------------------------------------------------------------------------
// <Driver::GetNodeManufacturerId>
// Get the manufacturer Id string value with the specified ID
// Returns a copy of the string rather than a const ref for thread safety
//-----------------------------------------------------------------------------
uint16 Driver::GetNodeManufacturerId(uint8 const _nodeId)
{
	Internal::LockGuard LG(m_nodeMutex);
	if (Node* node = GetNode(_nodeId))
	{
		return node->GetManufacturerId();
	}

	return 0;
}

//-----------------------------------------------------------------------------
// <Driver::GetNodeProductType>
// Get the product type string value with the specified ID
// Returns a copy of the string rather than a const ref for thread safety
//-----------------------------------------------------------------------------
uint16 Driver::GetNodeProductType(uint8 const _nodeId)
{
	Internal::LockGuard LG(m_nodeMutex);
	if (Node* node = GetNode(_nodeId))
	{
		return node->GetProductType();
	}

	return 0;
}

//-----------------------------------------------------------------------------
// <Driver::GetNodeProductId>
// Get the product Id string value with the specified ID
// Returns a copy of the string rather than a const ref for thread safety
//-----------------------------------------------------------------------------
uint16 Driver::GetNodeProductId(uint8 const _nodeId)
{
	Internal::LockGuard LG(m_nodeMutex);
	if (Node* node = GetNode(_nodeId))
	{
		return node->GetProductId();
	}

	return 0;
}

//-----------------------------------------------------------------------------
// <Driver::GetNodeDeviceType>
// Get the node device type as reported in the Z-Wave+ Info report.
//-----------------------------------------------------------------------------
uint16 Driver::GetNodeDeviceType(uint8 const _nodeId)
{
	Internal::LockGuard LG(m_nodeMutex);
	if (Node* node = GetNode(_nodeId))
	{
		return node->GetDeviceType();
	}

	return 0x00; // unknown
}

//-----------------------------------------------------------------------------
// <Driver::GetNodeDeviceTypeString>
// Get the node DeviceType as a string as reported in the Z-Wave+ Info report.
//-----------------------------------------------------------------------------

string Driver::GetNodeDeviceTypeString(uint8 const _nodeId)
{

	Internal::LockGuard LG(m_nodeMutex);
	if (Node* node = GetNode(_nodeId))
	{
		return node->GetDeviceTypeString();
	}

	return ""; // unknown
}

//-----------------------------------------------------------------------------
// <Driver::GetNodeRole>
// Get the node role as reported in the Z-Wave+ Info report.
//-----------------------------------------------------------------------------
uint8 Driver::GetNodeRole(uint8 const _nodeId)
{
	Internal::LockGuard LG(m_nodeMutex);
	if (Node* node = GetNode(_nodeId))
	{
		return node->GetRoleType();
	}

	return 0x00; // unknown
}

//-----------------------------------------------------------------------------
// <Driver::GetNodeRoleString>
// Get the node role as a string as reported in the Z-Wave+ Info report.
//-----------------------------------------------------------------------------
string Driver::GetNodeRoleString(uint8 const _nodeId)
{
	Internal::LockGuard LG(m_nodeMutex);
	if (Node* node = GetNode(_nodeId))
	{
		return node->GetRoleTypeString();
	}

	return ""; // unknown
}

//-----------------------------------------------------------------------------
// <Driver::GetNodePlusType>
// Get the node role as a string as reported in the Z-Wave+ Info report.
//-----------------------------------------------------------------------------
uint8 Driver::GetNodePlusType(uint8 const _nodeId)
{
	Internal::LockGuard LG(m_nodeMutex);
	if (Node* node = GetNode(_nodeId))
	{
		return node->GetNodeType();
	}
	return 0x00; // unknown
}

//-----------------------------------------------------------------------------
// <Driver::GetNodePlusTypeString>
// Get the node role as a string as reported in the Z-Wave+ Info report.
//-----------------------------------------------------------------------------
string Driver::GetNodePlusTypeString(uint8 const _nodeId)
{
	Internal::LockGuard LG(m_nodeMutex);
	if (Node* node = GetNode(_nodeId))
	{
		return node->GetNodeTypeString();
	}
	return ""; // unknown
}

//-----------------------------------------------------------------------------
// <Driver::SetNodeManufacturerName>
// Set the manufacturer name for the node with the specified ID
//-----------------------------------------------------------------------------
void Driver::SetNodeManufacturerName(uint8 const _nodeId, string const& _manufacturerName)
{
	Internal::LockGuard LG(m_nodeMutex);
	if (Node* node = GetNode(_nodeId))
	{
		node->SetManufacturerName(_manufacturerName);
	}
	WriteCache();
}

//-----------------------------------------------------------------------------
// <Driver::SetNodeProductName>
// Set the product name string value with the specified ID
//-----------------------------------------------------------------------------
void Driver::SetNodeProductName(uint8 const _nodeId, string const& _productName)
{
	Internal::LockGuard LG(m_nodeMutex);
	if (Node* node = GetNode(_nodeId))
	{
		node->SetProductName(_productName);
	}
	WriteCache();
}

//-----------------------------------------------------------------------------
// <Driver::SetNodeName>
// Set the node name string value with the specified ID
//-----------------------------------------------------------------------------
void Driver::SetNodeName(uint8 const _nodeId, string const& _nodeName)
{
	Internal::LockGuard LG(m_nodeMutex);
	if (Node* node = GetNode(_nodeId))
	{
		node->SetNodeName(_nodeName);
	}
	WriteCache();
}

//-----------------------------------------------------------------------------
// <Driver::SetNodeLocation>
// Set the location string value with the specified ID
//-----------------------------------------------------------------------------
void Driver::SetNodeLocation(uint8 const _nodeId, string const& _location)
{
	Internal::LockGuard LG(m_nodeMutex);
	if (Node* node = GetNode(_nodeId))
	{
		node->SetLocation(_location);
	}
	WriteCache();
}

//-----------------------------------------------------------------------------
// <Driver::SetNodeLevel>
// Helper to set the node level through the basic command class
//-----------------------------------------------------------------------------
void Driver::SetNodeLevel(uint8 const _nodeId, uint8 const _level)
{
	Internal::LockGuard LG(m_nodeMutex);
	if (Node* node = GetNode(_nodeId))
	{
		node->SetLevel(_level);
	}
}

//-----------------------------------------------------------------------------
// <Driver::SetNodeOn>
// Helper to set the node on through the basic command class
//-----------------------------------------------------------------------------
void Driver::SetNodeOn(uint8 const _nodeId)
{
	Internal::LockGuard LG(m_nodeMutex);
	if (Node* node = GetNode(_nodeId))
	{
		node->SetNodeOn();
	}
}

//-----------------------------------------------------------------------------
// <Driver::SetNodeOff>
// Helper to set the node off through the basic command class
//-----------------------------------------------------------------------------
void Driver::SetNodeOff(uint8 const _nodeId)
{
	Internal::LockGuard LG(m_nodeMutex);
	if (Node* node = GetNode(_nodeId))
	{
		node->SetNodeOff();
	}
}

//-----------------------------------------------------------------------------
// <Driver::GetValue>
// Get a pointer to a Value object for the specified ValueID
//-----------------------------------------------------------------------------
Internal::VC::Value* Driver::GetValue(ValueID const& _id)
{

	// This method is only called by code that has already locked the node
	if (Node* node = m_nodes[_id.GetNodeId()])
	{
		return node->GetValue(_id);
	}

	return NULL;
}

//-----------------------------------------------------------------------------
// Controller commands
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// <Driver::ResetController>
// Reset controller and erase all node information
//-----------------------------------------------------------------------------
void Driver::ResetController(Internal::Platform::Event* _evt)
{
	Log::Write(LogLevel_Info, "Reset controller and erase all node information");
	TODO(zway_controller_set_default(zway.....))
}

//-----------------------------------------------------------------------------
// <Driver::RequestNodeNeighbors>
// Get the neighbour information for a node from the controller
//-----------------------------------------------------------------------------
void Driver::RequestNodeNeighbors(uint8 const _nodeId, uint32 const _requestFlags)
{
	if (IsAPICallSupported( FUNC_ID_ZW_GET_ROUTING_INFO))
	{
		// Note: This is not the same as RequestNodeNeighbourUpdate.  This method
		// merely requests the controller's current neighbour information and
		// the reply will be copied into the relevant Node object for later use.
		Log::Write(LogLevel_Detail, "Requesting routing info (neighbor list) for Node %d", _nodeId);
		Internal::Msg* msg = new Internal::Msg("Get Routing Info", _nodeId, REQUEST, FUNC_ID_ZW_GET_ROUTING_INFO, false);
		msg->Append(_nodeId);
		msg->Append(0); // don't remove bad links
		msg->Append(0); // don't remove non-repeaters
		msg->Append(3); // funcid
		SendMsg(msg, MsgQueue_Command);
	}
}

//-----------------------------------------------------------------------------

TODO(Move this in Manager::AddNode ::RemoveNode ...)
/*
		Notification* notification = new Notification(Notification::Type_ControllerCommand);

        // PR #1879
        // The change below sets the nodeId in the notifications for controller state changes. These state changes are
        // caused by controller commands. Below is a list of controller commands with what the nodeId gets set to,
        // along with the Manager method(s) that use the controller command.

        // Driver::ControllerCommand_RequestNodeNeighborUpdate: supplied nodeId (Manager::HealNetworkNode, Manager::HealNetwork)
        // Driver::ControllerCommand_AddDevice: nodeId of an added node (Manager::AddNode)
        // Driver::ControllerCommand_RemoveDevice: nodeId of a removed node (Manager::RemoveNode)
        // Driver::ControllerCommand_RemoveFailedNode: supplied nodeId (Manager::RemoveFailedNode)
        // Driver::ControllerCommand_HasNodeFailed supplied nodeId (Manager::HasNodeFailed)
        // Driver::ControllerCommand_AssignReturnRoute: supplied nodeId (Manager::AssignReturnRoute)
        // Driver::ControllerCommand_RequestNodeNeighborUpdate: supplied nodeId (Manager::RequestNodeNeighborUpdate)
        // Driver::ControllerCommand_DeleteAllReturnRoutes supplied nodeId (Manager::DeleteAllReturnRoutes)
        // Driver::ControllerCommand_SendNodeInformation: supplied nodeId (Manager::SendNodeInformation)
        // Driver::ControllerCommand_CreateNewPrimary: unknown (Manager::CreateNewPrimary)
        // Driver::ControllerCommand_ReceiveConfiguration: unknown (Manager::ReceiveConfiguration)
        // Driver::ControllerCommand_ReplaceFailedNode: could be the supplied nodeId or the nodeId of the node that was added (Manager::ReplaceFailedNode)
        // Driver::ControllerCommand_TransferPrimaryRole: unknown (Manager::TransferPrimaryRole)
        // Driver::ControllerCommand_RequestNetworkUpdate: supplied nodeId (Manager::RequestNetworkUpdate)
        // Driver::ControllerCommand_ReplicationSend: supplied nodeId (Manager::ReplicationSend)
        // Driver::ControllerCommand_CreateButton: supplied nodeId (Manager::CreateButton)
        // Driver::ControllerCommand_DeleteButton: supplied nodeId (Manager::DeleteButton)
		notification->SetHomeAndNodeIds(m_homeId, m_currentControllerCommand->m_controllerCommandNode);

		notification->SetCommand(m_currentControllerCommand->m_controllerCommand);
		notification->SetEvent(_state);

		if (_error != ControllerError_None)
		{
			m_currentControllerCommand->m_controllerReturnError = _error;
			notification->SetNotification(_error);
		}
		QueueNotification(notification);
*/

//-----------------------------------------------------------------------------
// <Driver::TestNetwork>
// Run a series of messages to a single node or every node on the network.
//-----------------------------------------------------------------------------
void Driver::TestNetwork(uint8 const _nodeId, uint32 const _count)
{
	Internal::LockGuard LG(m_nodeMutex);
	if (_nodeId == 0)	// send _count messages to every node
	{
		for (int i = 0; i < 256; ++i)
		{
			if (i == m_Controller_nodeId) // ignore sending to ourself
			{
				continue;
			}
			if (m_nodes[i] != NULL)
			{
				Internal::CC::NoOperation *noop = static_cast<Internal::CC::NoOperation*>(m_nodes[i]->GetCommandClass(Internal::CC::NoOperation::StaticGetCommandClassId()));
				for (int j = 0; j < (int) _count; j++)
				{
					noop->Set(true);
				}
			}
		}
	}
	else if (_nodeId != m_Controller_nodeId && m_nodes[_nodeId] != NULL)
	{
		Internal::CC::NoOperation *noop = static_cast<Internal::CC::NoOperation*>(m_nodes[_nodeId]->GetCommandClass(Internal::CC::NoOperation::StaticGetCommandClassId()));
		for (int i = 0; i < (int) _count; i++)
		{
			noop->Set(true);
		}
	}
}

//-----------------------------------------------------------------------------
//	SwitchAll
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// <Driver::SwitchAllOn>
// All devices that support the SwitchAll command class will be turned on
//-----------------------------------------------------------------------------
void Driver::SwitchAllOn()
{
	Internal::CC::SwitchAll::On(this, 0xff);

	Internal::LockGuard LG(m_nodeMutex);
	for (int i = 0; i < 256; ++i)
	{
		if (GetNodeUnsafe(i))
		{
			if (m_nodes[i]->GetCommandClass(Internal::CC::SwitchAll::StaticGetCommandClassId()))
			{
				Internal::CC::SwitchAll::On(this, (uint8) i);
			}
		}
	}
}

//-----------------------------------------------------------------------------
// <Driver::SwitchAllOff>
// All devices that support the SwitchAll command class will be turned off
//-----------------------------------------------------------------------------
void Driver::SwitchAllOff()
{
	Internal::CC::SwitchAll::Off(this, 0xff);

	Internal::LockGuard LG(m_nodeMutex);
	for (int i = 0; i < 256; ++i)
	{
		if (GetNodeUnsafe(i))
		{
			if (m_nodes[i]->GetCommandClass(Internal::CC::SwitchAll::StaticGetCommandClassId()))
			{
				Internal::CC::SwitchAll::Off(this, (uint8) i);
			}
		}
	}
}

//-----------------------------------------------------------------------------
// <Driver::SetConfigParam>
// Set the value of one of the configuration parameters of a device
//-----------------------------------------------------------------------------
bool Driver::SetConfigParam(uint8 const _nodeId, uint8 const _param, int32 _value, uint8 _size)
{
	Internal::LockGuard LG(m_nodeMutex);
	if (Node* node = GetNode(_nodeId))
	{
		return node->SetConfigParam(_param, _value, _size);
	}

	return false;
}

//-----------------------------------------------------------------------------
// <Driver::RequestConfigParam>
// Request the value of one of the configuration parameters of a device
//-----------------------------------------------------------------------------
void Driver::RequestConfigParam(uint8 const _nodeId, uint8 const _param)
{
	Internal::LockGuard LG(m_nodeMutex);
	if (Node* node = GetNode(_nodeId))
	{
		node->RequestConfigParam(_param);
	}
}

//-----------------------------------------------------------------------------
// <Driver::GetNumGroups>
// Gets the number of association groups reported by this node
//-----------------------------------------------------------------------------
uint8 Driver::GetNumGroups(uint8 const _nodeId)
{
	uint8 numGroups = 0;
	Internal::LockGuard LG(m_nodeMutex);
	if (Node* node = GetNode(_nodeId))
	{
		numGroups = node->GetNumGroups();
	}

	return numGroups;
}

//-----------------------------------------------------------------------------
// <Driver::GetAssociations>
// Gets the associations for a group
//-----------------------------------------------------------------------------
uint32 Driver::GetAssociations(uint8 const _nodeId, uint8 const _groupIdx, uint8** o_associations)
{
	uint32 numAssociations = 0;
	Internal::LockGuard LG(m_nodeMutex);
	if (Node* node = GetNode(_nodeId))
	{
		numAssociations = node->GetAssociations(_groupIdx, o_associations);
	}

	return numAssociations;
}

//-----------------------------------------------------------------------------
// <Driver::GetAssociations>
// Gets the associations for a group
//-----------------------------------------------------------------------------
uint32 Driver::GetAssociations(uint8 const _nodeId, uint8 const _groupIdx, InstanceAssociation** o_associations)
{
	uint32 numAssociations = 0;
	Internal::LockGuard LG(m_nodeMutex);
	if (Node* node = GetNode(_nodeId))
	{
		numAssociations = node->GetAssociations(_groupIdx, o_associations);
	}

	return numAssociations;
}

//-----------------------------------------------------------------------------
// <Driver::GetMaxAssociations>
// Gets the maximum number of associations for a group
//-----------------------------------------------------------------------------
uint8 Driver::GetMaxAssociations(uint8 const _nodeId, uint8 const _groupIdx)
{
	uint8 maxAssociations = 0;
	Internal::LockGuard LG(m_nodeMutex);
	if (Node* node = GetNode(_nodeId))
	{
		maxAssociations = node->GetMaxAssociations(_groupIdx);
	}

	return maxAssociations;
}

//-----------------------------------------------------------------------------
// <Driver::IsMultiInstance>
// Returns true if group supports multi instance
//-----------------------------------------------------------------------------
bool Driver::IsMultiInstance(uint8 const _nodeId, uint8 const _groupIdx)
{
	bool multiInstance = false;
	Internal::LockGuard LG(m_nodeMutex);
	if (Node* node = GetNode(_nodeId))
	{
		multiInstance = node->IsMultiInstance(_groupIdx);
	}
	return multiInstance;
}

//-----------------------------------------------------------------------------
// <Driver::GetGroupLabel>
// Gets the label for a particular group
//-----------------------------------------------------------------------------
string Driver::GetGroupLabel(uint8 const _nodeId, uint8 const _groupIdx)
{
	string label = "";
	Internal::LockGuard LG(m_nodeMutex);
	if (Node* node = GetNode(_nodeId))
	{
		label = node->GetGroupLabel(_groupIdx);
	}

	return label;
}

//-----------------------------------------------------------------------------
// <Driver::AddAssociation>
// Adds a node to an association group
//-----------------------------------------------------------------------------
void Driver::AddAssociation(uint8 const _nodeId, uint8 const _groupIdx, uint8 const _targetNodeId, uint8 const _instance)
{
	Internal::LockGuard LG(m_nodeMutex);
	if (Node* node = GetNode(_nodeId))
	{
		node->AddAssociation(_groupIdx, _targetNodeId, _instance);
	}
}

//-----------------------------------------------------------------------------
// <Driver::RemoveAssociation>
// Removes a node from an association group
//-----------------------------------------------------------------------------
void Driver::RemoveAssociation(uint8 const _nodeId, uint8 const _groupIdx, uint8 const _targetNodeId, uint8 const _instance)
{
	Internal::LockGuard LG(m_nodeMutex);
	if (Node* node = GetNode(_nodeId))
	{
		node->RemoveAssociation(_groupIdx, _targetNodeId, _instance);
	}
}

//-----------------------------------------------------------------------------
// <Driver::QueueNotification>
// Add a notification to the queue to be sent at a later, safe time.
//-----------------------------------------------------------------------------
void Driver::QueueNotification(Notification* _notification)
{
	m_notifications.push_back(_notification);
	m_notificationsEvent->Set();
}

//-----------------------------------------------------------------------------
// <Driver::NotifyWatchers>
// Notify any watching objects of a value change
//-----------------------------------------------------------------------------
void Driver::NotifyWatchers()
{
	list<Notification*>::iterator nit = m_notifications.begin();
	while (nit != m_notifications.end())
	{
		Notification* notification = m_notifications.front();
		m_notifications.pop_front();

		/* check the any ValueID's sent as part of the Notification are still valid */
		switch (notification->GetType())
		{
			case Notification::Type_ValueAdded:
			case Notification::Type_ValueChanged:
			case Notification::Type_ValueRefreshed:
			{
				Internal::VC::Value *val = GetValue(notification->GetValueID());
				if (!val)
				{
					Log::Write(LogLevel_Info, notification->GetNodeId(), "Dropping Notification as ValueID does not exist");
					nit = m_notifications.begin();
					delete notification;
					continue;
				}
				val->Release();
				break;
			}
			default:
				break;
		}
		Log::Write(LogLevel_Detail, notification->GetNodeId(), "Notification: %s", notification->GetAsString().c_str());

		Manager::Get()->NotifyWatchers(notification);

		delete notification;
		nit = m_notifications.begin();
	}
	m_notificationsEvent->Reset();
}

//-----------------------------------------------------------------------------
// <Driver::SaveButtons>
// Save button info into file.
//-----------------------------------------------------------------------------
void Driver::SaveButtons()
{
	char str[16];

	// Create a new XML document to contain the driver configuration
	TiXmlDocument doc;
	TiXmlDeclaration* decl = new TiXmlDeclaration("1.0", "utf-8", "");
	TiXmlElement* nodesElement = new TiXmlElement("Nodes");
	doc.LinkEndChild(decl);
	doc.LinkEndChild(nodesElement);

	nodesElement->SetAttribute("xmlns", "http://code.google.com/p/open-zwave/");

	snprintf(str, sizeof(str), "%d", 1);
	nodesElement->SetAttribute("version", str);
	Internal::LockGuard LG(m_nodeMutex);
	for (int i = 1; i < 256; i++)
	{
		if (m_nodes[i] == NULL || m_nodes[i]->m_buttonMap.empty())
		{
			continue;
		}

		TiXmlElement* nodeElement = new TiXmlElement("Node");

		snprintf(str, sizeof(str), "%d", i);
		nodeElement->SetAttribute("id", str);

		for (map<uint8, uint8>::iterator it = m_nodes[i]->m_buttonMap.begin(); it != m_nodes[i]->m_buttonMap.end(); ++it)
		{
			TiXmlElement* valueElement = new TiXmlElement("Button");

			snprintf(str, sizeof(str), "%d", it->first);
			valueElement->SetAttribute("id", str);

			snprintf(str, sizeof(str), "%d", it->second);
			TiXmlText* textElement = new TiXmlText(str);
			valueElement->LinkEndChild(textElement);

			nodeElement->LinkEndChild(valueElement);
		}

		nodesElement->LinkEndChild(nodeElement);
	}

	string userPath;
	Options::Get()->GetOptionAsString("UserPath", &userPath);

	string filename = userPath + "zwbutton.xml";

	doc.SaveFile(filename.c_str());
}
//-----------------------------------------------------------------------------
// <Driver::ReadButtons>
// Read button info per node from file.
//-----------------------------------------------------------------------------
void Driver::ReadButtons(uint8 const _nodeId)
{
	int32 intVal;
	int32 nodeId;
	int32 buttonId;
	char const* str;

	// Load the XML document that contains the driver configuration
	string userPath;
	Options::Get()->GetOptionAsString("UserPath", &userPath);

	string filename = userPath + "zwbutton.xml";

	TiXmlDocument doc;
	if (!doc.LoadFile(filename.c_str(), TIXML_ENCODING_UTF8))
	{
		Log::Write(LogLevel_Debug, "Driver::ReadButtons - zwbutton.xml file not found.");
		return;
	}
	doc.SetUserData((void *) filename.c_str());
	TiXmlElement const* nodesElement = doc.RootElement();
	str = nodesElement->Value();
	if (str && strcmp(str, "Nodes"))
	{
		Log::Write(LogLevel_Warning, "WARNING: Driver::ReadButtons - zwbutton.xml is malformed");
		return;
	}

	// Version
	if (TIXML_SUCCESS == nodesElement->QueryIntAttribute("version", &intVal))
	{
		if ((uint32) intVal != 1)
		{
			Log::Write(LogLevel_Info, "Driver::ReadButtons - %s is from an older version of OpenZWave and cannot be loaded.", "zwbutton.xml");
			return;
		}
	}
	else
	{
		Log::Write(LogLevel_Warning, "WARNING: Driver::ReadButtons - zwbutton.xml is from an older version of OpenZWave and cannot be loaded.");
		return;
	}

	TiXmlElement const* nodeElement = nodesElement->FirstChildElement();
	while (nodeElement)
	{
		str = nodeElement->Value();
		if (str && !strcmp(str, "Node"))
		{
			Node* node = NULL;
			if (TIXML_SUCCESS == nodeElement->QueryIntAttribute("id", &intVal))
			{
				if (_nodeId == intVal)
				{
					node = GetNodeUnsafe(intVal);
				}
			}
			if (node != NULL)
			{
				TiXmlElement const* buttonElement = nodeElement->FirstChildElement();
				while (buttonElement)
				{
					str = buttonElement->Value();
					if (str && !strcmp(str, "Button"))
					{
						if (TIXML_SUCCESS != buttonElement->QueryIntAttribute("id", &buttonId))
						{
							Log::Write(LogLevel_Warning, "WARNING: Driver::ReadButtons - cannot find Button Id for node %d", _nodeId);
							return;
						}
						str = buttonElement->GetText();
						if (str)
						{
							char *p;
							nodeId = (int32) strtol(str, &p, 0);
						}
						else
						{
							Log::Write(LogLevel_Info, "Driver::ReadButtons - missing virtual node value for node %d button id %d", _nodeId, buttonId);
							return;
						}
						node->m_buttonMap[buttonId] = nodeId;
						Notification* notification = new Notification(Notification::Type_CreateButton);
						notification->SetHomeAndNodeIds(m_homeId, nodeId);
						notification->SetButtonId(buttonId);
						QueueNotification(notification);
					}
					buttonElement = buttonElement->NextSiblingElement();
				}
			}
		}
		nodeElement = nodeElement->NextSiblingElement();
	}
}

//-----------------------------------------------------------------------------
// <Driver::GetDriverStatistics>
// Return driver statistics
//-----------------------------------------------------------------------------
void Driver::GetDriverStatistics(DriverData* _data)
{
	_data->m_SOFCnt = m_SOFCnt;
	_data->m_ACKWaiting = m_ACKWaiting;
	_data->m_readAborts = m_readAborts;
	_data->m_badChecksum = m_badChecksum;
	_data->m_readCnt = m_readCnt;
	_data->m_writeCnt = m_writeCnt;
	_data->m_CANCnt = m_CANCnt;
	_data->m_NAKCnt = m_NAKCnt;
	_data->m_ACKCnt = m_ACKCnt;
	_data->m_OOFCnt = m_OOFCnt;
	_data->m_dropped = m_dropped;
	_data->m_retries = m_retries;
	_data->m_callbacks = m_callbacks;
	_data->m_badroutes = m_badroutes;
	_data->m_noack = m_noack;
	_data->m_netbusy = m_netbusy;
	_data->m_notidle = m_notidle;
	_data->m_txverified = m_txverified;
	_data->m_nondelivery = m_nondelivery;
	_data->m_routedbusy = m_routedbusy;
	_data->m_broadcastReadCnt = m_broadcastReadCnt;
	_data->m_broadcastWriteCnt = m_broadcastWriteCnt;
}

//-----------------------------------------------------------------------------
// <Driver::GetNodeStatistics>
// Return per node statistics
//-----------------------------------------------------------------------------
void Driver::GetNodeStatistics(uint8 const _nodeId, Node::NodeData* _data)
{
	Internal::LockGuard LG(m_nodeMutex);
	Node* node = GetNode(_nodeId);
	if (node != NULL)
	{
		node->GetNodeStatistics(_data);
	}
}

//-----------------------------------------------------------------------------
// <Driver::LogDriverStatistics>
// Report driver statistics to the driver's log
//-----------------------------------------------------------------------------
void Driver::LogDriverStatistics()
{
	DriverData data;

	GetDriverStatistics(&data);
	int32 totalElapsed = -m_startTime.TimeRemaining();
	int32 days = totalElapsed / (1000 * 60 * 60 * 24);

	totalElapsed -= days * 1000 * 60 * 60 * 24;
	int32 hours = totalElapsed / (1000 * 60 * 60);

	totalElapsed -= hours * 1000 * 60 * 60;
	int32 minutes = totalElapsed / (1000 * 60);

	Log::Write(LogLevel_Always, "***************************************************************************");
	Log::Write(LogLevel_Always, "*********************  Cumulative Network Statistics  *********************");
	Log::Write(LogLevel_Always, "*** General");
	Log::Write(LogLevel_Always, "Driver run time: . .  . %ld days, %ld hours, %ld minutes", days, hours, minutes);
	Log::Write(LogLevel_Always, "Frames processed: . . . . . . . . . . . . . . . . . . . . %ld", data.m_SOFCnt);
	Log::Write(LogLevel_Always, "Total messages successfully received: . . . . . . . . . . %ld", data.m_readCnt);
	Log::Write(LogLevel_Always, "Total Messages successfully sent: . . . . . . . . . . . . %ld", data.m_writeCnt);
	Log::Write(LogLevel_Always, "ACKs received from controller:  . . . . . . . . . . . . . %ld", data.m_ACKCnt);
	// Consider tracking and adding:
	//		Initialization messages
	//		Ad-hoc command messages
	//		Polling messages
	//		Messages inititated by network
	//		Others?
	Log::Write(LogLevel_Always, "*** Errors");
	Log::Write(LogLevel_Always, "Unsolicited messages received while waiting for ACK:  . . %ld", data.m_ACKWaiting);
	Log::Write(LogLevel_Always, "Reads aborted due to timeouts:  . . . . . . . . . . . . . %ld", data.m_readAborts);
	Log::Write(LogLevel_Always, "Bad checksum errors:  . . . . . . . . . . . . . . . . . . %ld", data.m_badChecksum);
	Log::Write(LogLevel_Always, "CANs received from controller:  . . . . . . . . . . . . . %ld", data.m_CANCnt);
	Log::Write(LogLevel_Always, "NAKs received from controller:  . . . . . . . . . . . . . %ld", data.m_NAKCnt);
	Log::Write(LogLevel_Always, "Out of frame data flow errors:  . . . . . . . . . . . . . %ld", data.m_OOFCnt);
	Log::Write(LogLevel_Always, "Messages retransmitted: . . . . . . . . . . . . . . . . . %ld", data.m_retries);
	Log::Write(LogLevel_Always, "Messages dropped and not delivered: . . . . . . . . . . . %ld", data.m_dropped);
	Log::Write(LogLevel_Always, "***************************************************************************");
}

//-----------------------------------------------------------------------------
// <Driver::GetNetworkKey>
// Get the Network Key we will use for Security Command Class
//-----------------------------------------------------------------------------
uint8 *Driver::GetNetworkKey()
{
	std::string networkKey;
	std::vector<std::string> elems;
	unsigned int tempkey[16];
	static uint8 keybytes[16] =
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
	static bool keySet = false;
	if (keySet == false)
	{
		Options::Get()->GetOptionAsString("NetworkKey", &networkKey);
		Internal::split(elems, networkKey, ",", true);
		if (elems.size() != 16)
		{
			Log::Write(LogLevel_Warning, "Invalid Network Key. Does not contain 16 Bytes - Contains %d", elems.size());
			Log::Write(LogLevel_Warning, "Raw Key: %s", networkKey.c_str());
			Log::Write(LogLevel_Warning, "Parsed Key:");
			int i = 0;
			for (std::vector<std::string>::iterator it = elems.begin(); it != elems.end(); it++)
				Log::Write(LogLevel_Warning, "%d) - %s", ++i, (*it).c_str());
			OZW_FATAL_ERROR(OZWException::OZWEXCEPTION_SECURITY_FAILED, "Failed to Read Network Key");
		}
		int i = 0;
		for (std::vector<std::string>::iterator it = elems.begin(); it != elems.end(); it++)
		{
			if (0 == sscanf(Internal::trim(*it).c_str(), "%x", &tempkey[i]))
			{
				Log::Write(LogLevel_Warning, "Cannot Convert Network Key Byte %s to Key", (*it).c_str());
				OZW_FATAL_ERROR(OZWException::OZWEXCEPTION_SECURITY_FAILED, "Failed to Convert Network Key");
			}
			else
			{
				keybytes[i] = (tempkey[i] & 0xFF);
			}
			i++;
		}
		keySet = true;
	}
	return keybytes;
}

bool Driver::isNetworkKeySet()
{
	std::string networkKey;
	if (!Options::Get()->GetOptionAsString("NetworkKey", &networkKey))
	{
		return false;
	}
	else
	{
		return networkKey.length() <= 0 ? false : true;
	}
}

bool Driver::CheckNodeConfigRevision(Node *node)
{
	Internal::DNSLookup *lu = new Internal::DNSLookup;
	lu->NodeID = node->GetNodeId();
	/* make up a string of what we want to look up */
	std::stringstream ss;
	ss << std::hex << std::setw(4) << std::setfill('0') << node->GetProductId() << ".";
	ss << std::hex << std::setw(4) << std::setfill('0') << node->GetProductType() << ".";
	ss << std::hex << std::setw(4) << std::setfill('0') << node->GetManufacturerId() << ".db.openzwave.com";

	lu->lookup = ss.str();
	lu->type = Internal::DNS_Lookup_ConfigRevision;
	return m_dns->sendRequest(lu);
}

bool Driver::CheckMFSConfigRevision()
{
	Internal::DNSLookup *lu = new Internal::DNSLookup;
	lu->NodeID = 0;
	lu->lookup = "mfs.db.openzwave.com";
	lu->type = Internal::DNS_Lookup_ConfigRevision;
	return m_dns->sendRequest(lu);
}

void Driver::processConfigRevision(Internal::DNSLookup *result)
{
	if (result->status == Internal::Platform::DNSError_None)
	{
		if (result->type == Internal::DNS_Lookup_ConfigRevision)
		{
			if (result->NodeID > 0)
			{
				Internal::LockGuard LG(m_nodeMutex);
				Node *node = this->GetNode(result->NodeID);
				if (!node)
				{
					Log::Write(LogLevel_Warning, result->NodeID, "Node disappeared when processing Config Revision");
					return;
				}
				node->setLatestConfigRevision((unsigned long) atol(result->result.c_str()));
				if (node->getFileConfigRevision() < node->getLatestConfigRevision())
				{
					Log::Write(LogLevel_Warning, node->GetNodeId(), "Config File for Device \"%s\" is out of date", node->GetProductName().c_str());
					Notification* notification = new Notification(Notification::Type_UserAlerts);
					notification->SetHomeAndNodeIds(m_homeId, node->GetNodeId());
					notification->SetUserAlertNotification(Notification::Alert_ConfigOutOfDate);
					QueueNotification(notification);

					bool update = false;
					Options::Get()->GetOptionAsBool("AutoUpdateConfigFile", &update);

					if (update)
						m_mfs->updateConfigFile(this, node);

				}
			}
			else if (result->NodeID == 0)
			{
				/* manufacturer_specific */
				m_mfs->setLatestRevision((unsigned long) atol(result->result.c_str()));
				if (m_mfs->getRevision() < (unsigned long) atol(result->result.c_str()))
				{
					Log::Write(LogLevel_Warning, "Config Revision of ManufacturerSpecific Database is out of date");
					Notification* notification = new Notification(Notification::Type_UserAlerts);
					notification->SetUserAlertNotification(Notification::Alert_MFSOutOfDate);
					QueueNotification(notification);

					bool update = false;
					Options::Get()->GetOptionAsBool("AutoUpdateConfigFile", &update);

					if (update)
					{
						m_mfs->updateMFSConfigFile(this);
					}
					else
					{
						m_mfs->checkInitialized();
					}
				}
				else
				{
					/* its upto date - Check to make sure we have all the config files */
					m_mfs->checkConfigFiles(this);
				}
			}
			return;
		}
	}
	else if (result->status == Internal::Platform::DNSError_NotFound)
	{
		Log::Write(LogLevel_Info, "Not Found for Device record %s", result->lookup.c_str());
		Notification* notification = new Notification(Notification::Type_UserAlerts);
		notification->SetUserAlertNotification(Notification::Alert_DNSError);
		QueueNotification(notification);
	}
	else if (result->status == Internal::Platform::DNSError_DomainError)
	{
		Log::Write(LogLevel_Warning, "Domain Error Looking up record %s", result->lookup.c_str());
		Notification* notification = new Notification(Notification::Type_UserAlerts);
		notification->SetUserAlertNotification(Notification::Alert_DNSError);
		QueueNotification(notification);
	}
	else if (result->status == Internal::Platform::DNSError_InternalError)
	{
		Log::Write(LogLevel_Warning, "Internal DNS Error looking up record %s", result->lookup.c_str());
		Notification* notification = new Notification(Notification::Type_UserAlerts);
		notification->SetUserAlertNotification(Notification::Alert_DNSError);
		QueueNotification(notification);
	}
	m_mfs->checkInitialized();
}

bool Driver::setHttpClient(Internal::i_HttpClient *client)
{
	if (m_httpClient)
		delete m_httpClient;
	m_httpClient = client;
	return true;
}

bool Driver::startConfigDownload(uint16 _manufacturerId, uint16 _productType, uint16 _productId, string configfile, uint8 node)
{
	Internal::HttpDownload *download = new Internal::HttpDownload();
	std::stringstream ss;
	ss << std::hex << std::setw(4) << std::setfill('0') << _productId << ".";
	ss << std::hex << std::setw(4) << std::setfill('0') << _productType << ".";
	ss << std::hex << std::setw(4) << std::setfill('0') << _manufacturerId << ".xml";
	download->url = "http://download.db.openzwave.com/" + ss.str();
	download->filename = configfile;
	download->operation = Internal::HttpDownload::Config;
	download->node = node;
	Log::Write(LogLevel_Info, "Queuing download for %s (Node %d)", download->url.c_str(), download->node);

	return m_httpClient->StartDownload(download);
}

bool Driver::startMFSDownload(string configfile)
{
	Internal::HttpDownload *download = new Internal::HttpDownload();
	download->url = "http://download.db.openzwave.com/mfs.xml";
	download->filename = configfile;
	download->operation = Internal::HttpDownload::MFSConfig;
	download->node = 0;
	Log::Write(LogLevel_Info, "Queuing download for %s", download->url.c_str());

	return m_httpClient->StartDownload(download);
}

bool Driver::refreshNodeConfig(uint8 _nodeId)
{
	Internal::LockGuard LG(m_nodeMutex);
	string action;
	Options::Get()->GetOptionAsString("ReloadAfterUpdate", &action);
	if (Internal::ToUpper(action) == "NEVER")
	{
		Notification* notification = new Notification(Notification::Type_UserAlerts);
		notification->SetUserAlertNotification(Notification::Alert_NodeReloadRequired);
		QueueNotification(notification);
		return true;
	}
	else if (Internal::ToUpper(action) == "IMMEDIATE")
	{
		Log::Write(LogLevel_Info, _nodeId, "Reloading Node after new Config File loaded");
		/* this will reload the Node, ignoring any cache that exists etc */
		ReloadNode(_nodeId);
		return true;
	}
	else if (Internal::ToUpper(action) == "AWAKE")
	{
		Node *node = GetNode(_nodeId);
		if (!node->IsListeningDevice())
		{
			if (Internal::CC::WakeUp* wakeUp = static_cast<Internal::CC::WakeUp*>(node->GetCommandClass(Internal::CC::WakeUp::StaticGetCommandClassId())))
			{
				if (!wakeUp->IsAwake())
				{
					/* Node is Asleep. Queue it for WakeUp */
					Log::Write(LogLevel_Info, _nodeId, "Queuing Sleeping Node Reload after New Config File Loaded");
					MsgQueueItem item;
					item.m_command = MsgQueueCmd_ReloadNode;
					item.m_nodeId = _nodeId;
					wakeUp->QueueMsg(item);
				}
				else
				{
					/* Node is Awake. Reload it */
					Log::Write(LogLevel_Info, _nodeId, "Reloading Awake Node after new Config File loaded");
					ReloadNode(_nodeId);
					return true;
				}
			}
		}
		else
		{
			Log::Write(LogLevel_Info, _nodeId, "Reloading Node after new Config File Loaded");
			ReloadNode(_nodeId);
		}
	}
	return false;
}

//-----------------------------------------------------------------------------
// <Driver::ReloadNode>
// Reload a Node - Remove it from ozwcache, and re-initilize the node from scratch (doing a full  interview)
//-----------------------------------------------------------------------------
void Driver::ReloadNode(uint8 const _nodeId)
{
	Internal::LockGuard LG(m_nodeMutex);
	Log::Write(LogLevel_Detail, _nodeId, "Reloading Node");
	/* delete any cached information about this node so we start from fresh */
	char str[32];
	int32 intVal;

	string userPath;
	Options::Get()->GetOptionAsString("UserPath", &userPath);

	snprintf(str, sizeof(str), "ozwcache_0x%08x.xml", m_homeId);
	string filename = userPath + string(str);

	TiXmlDocument doc;
	if (doc.LoadFile(filename.c_str(), TIXML_ENCODING_UTF8))
	{
		doc.SetUserData((void *) filename.c_str());
		TiXmlElement * driverElement = doc.RootElement();

		TiXmlNode * nodeElement = driverElement->FirstChild();
		while (nodeElement)
		{
			if (nodeElement->ToElement())
			{
				char const* str2 = nodeElement->ToElement()->Value();
				if (str2 && !strcmp(str2, "Node"))
				{
					// Get the node Id from the XML
					if (TIXML_SUCCESS == nodeElement->ToElement()->QueryIntAttribute("id", &intVal))
					{
						if (intVal == _nodeId)
						{
							driverElement->RemoveChild(nodeElement);
							break;
						}
					}
				}
			}
			nodeElement = nodeElement->NextSibling();
		}
	}
	doc.SaveFile(filename.c_str());
	LG.Unlock();

	InitNode(_nodeId);
}

void Driver::processDownload(Internal::HttpDownload *download)
{
	if (download->transferStatus == Internal::HttpDownload::Ok)
	{
		Log::Write(LogLevel_Info, "Download Finished: %s (Node: %d)", download->filename.c_str(), download->node);
		if (download->operation == Internal::HttpDownload::Config)
		{
			m_mfs->configDownloaded(this, download->filename, download->node);
		}
		else if (download->operation == Internal::HttpDownload::MFSConfig)
		{
			m_mfs->mfsConfigDownloaded(this, download->filename);
		}
	}
	else
	{
		Log::Write(LogLevel_Warning, "Download of %s Failed (Node: %d)", download->url.c_str(), download->node);
		if (download->operation == Internal::HttpDownload::Config)
		{
			m_mfs->configDownloaded(this, download->filename, download->node, false);
		}
		else if (download->operation == Internal::HttpDownload::MFSConfig)
		{
			m_mfs->mfsConfigDownloaded(this, download->filename, false);
		}
		Notification* notification = new Notification(Notification::Type_UserAlerts);
		notification->SetUserAlertNotification(Notification::Alert_ConfigFileDownloadFailed);
		QueueNotification(notification);
	}

}

bool Driver::downloadConfigRevision(Node *node)
{
	/* only download if the revision is 1 or higher. Revision 0's are for local testing only */
	if (node->getFileConfigRevision() <= 0)
	{
		Log::Write(LogLevel_Warning, node->GetNodeId(), "Config File Revision is 0. Not Updating");
		Notification* notification = new Notification(Notification::Type_UserAlerts);
		notification->SetUserAlertNotification(Notification::Alert_ConfigFileDownloadFailed);
		QueueNotification(notification);
		return false;
	}
	if (node->getFileConfigRevision() >= node->getLatestConfigRevision())
	{
		Log::Write(LogLevel_Warning, node->GetNodeId(), "Config File Revision %d is equal to or greater than current revision %d", node->getFileConfigRevision(), node->getLatestConfigRevision());
		Notification* notification = new Notification(Notification::Type_UserAlerts);
		notification->SetUserAlertNotification(Notification::Alert_ConfigFileDownloadFailed);
		QueueNotification(notification);
		return false;
	}
	else
	{
		m_mfs->updateConfigFile(this, node);
		return true;
	}
}
bool Driver::downloadMFSRevision()
{
	if (m_mfs->getRevision() <= 0)
	{
		Log::Write(LogLevel_Warning, "ManufacturerSpecific Revision is 0. Not Updating");
		Notification* notification = new Notification(Notification::Type_UserAlerts);
		notification->SetUserAlertNotification(Notification::Alert_ConfigFileDownloadFailed);
		QueueNotification(notification);
		return false;
	}
	if (m_mfs->getRevision() >= m_mfs->getLatestRevision())
	{
		Log::Write(LogLevel_Warning, "ManufacturerSpecific Revision %d is equal to or greater than current revision %d", m_mfs->getRevision(), m_mfs->getLatestRevision());
		Notification* notification = new Notification(Notification::Type_UserAlerts);
		notification->SetUserAlertNotification(Notification::Alert_ConfigFileDownloadFailed);
		QueueNotification(notification);
		return false;
	}
	m_mfs->updateMFSConfigFile(this);
	return true;
}

void Driver::SubmitEventMsg(EventMsg *event)
{
	Internal::LockGuard LG(m_eventMutex);
	m_eventQueueMsg.push_back(event);
	m_queueMsgEvent->Set();
}

void Driver::ProcessEventMsg()
{
	EventMsg *event;
	{
		Internal::LockGuard LG(m_eventMutex);
		event = m_eventQueueMsg.front();
		m_eventQueueMsg.pop_front();
		if (m_eventQueueMsg.empty())
			m_queueMsgEvent->Reset();
	}
	switch (event->type)
	{
		case EventMsg::Event_DNS:
			processConfigRevision(event->event.lookup);
			delete event->event.lookup;
			break;
		case EventMsg::Event_Http:
			processDownload(event->event.httpdownload);
			delete event->event.httpdownload;
			break;
	}
	delete event;
}

//-----------------------------------------------------------------------------
// <Manager::GetMetaData>
// Retrieve MetaData about a Node.
//-----------------------------------------------------------------------------
string const Driver::GetMetaData(uint8 const _nodeId, Node::MetaDataFields _metadata)
{
	Internal::LockGuard LG(m_nodeMutex);
	Node* node = GetNode(_nodeId);
	if (node != NULL)
	{
		return node->GetMetaData(_metadata);
	}
	return "";
}

//-----------------------------------------------------------------------------
// <Manager::GetMetaData>
// Retrieve MetaData about a Node.
//-----------------------------------------------------------------------------
Node::ChangeLogEntry const Driver::GetChangeLog(uint8 const _nodeId, uint32_t revision)
{
	LOG_CALL
	Internal::LockGuard LG(m_nodeMutex);
	Node* node = GetNode(_nodeId);
	if (node != NULL)
	{
		return node->GetChangeLog(revision);
	}
	Node::ChangeLogEntry cle;
	cle.revision = -1;
	return cle;
}

Internal::ManufacturerSpecificDB *Driver::GetManufacturerSpecificDB()
{
	LOG_CALL
	return this->m_mfs;
}

