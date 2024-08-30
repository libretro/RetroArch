//
//  ServerManager.swift
//  AltStore
//
//  Created by Riley Testut on 5/30/19.
//  Copyright © 2019 Riley Testut. All rights reserved.
//

import Foundation
import Network

import UIKit

@_exported import CAltKit

public enum ConnectionError: LocalizedError
{
    case serverNotFound
    case connectionFailed(Server)
    case connectionDropped(Server)
    case unknownUDID
    case unsupportedOS
    
    public var errorDescription: String? {
        switch self
        {
        case .serverNotFound: return NSLocalizedString("Could not find AltServer.", comment: "")
        case .connectionFailed: return NSLocalizedString("Could not connect to AltServer.", comment: "")
        case .connectionDropped: return NSLocalizedString("The connection to AltServer was dropped.", comment: "")
        case .unknownUDID: return NSLocalizedString("This device's UDID could not be determined.", comment: "")
        case .unsupportedOS: return NSLocalizedString("This device's OS version is too old to run AltKit.", comment: "")
        }
    }
}

@objc(ALTServerManager) @objcMembers
public class ServerManager: NSObject
{
    public static let shared = ServerManager()
    
    private(set) var isDiscovering = false
    private(set) var discoveredServers = [Server]()
    
    public var discoveredServerHandler: ((Server) -> Void)?
    public var lostServerHandler: ((Server) -> Void)?
    
    public var callbackQueue: DispatchQueue = .main
    
    // Allow other AltKit queues to target this one.
    internal let dispatchQueue = DispatchQueue(label: "io.altstore.altkit.ServerManager", qos: .utility, autoreleaseFrequency: .workItem)
    
    private var serviceBrowser: NetServiceBrowser?
    private var resolvingServices = Set<NetService>()
    
    private var autoconnectGroup: DispatchGroup?
    private var ignoredServers = Set<Server>()
    
    private override init()
    {
        super.init()
        
        NotificationCenter.default.addObserver(self, selector: #selector(ServerManager.didEnterBackground(_:)), name: UIApplication.didEnterBackgroundNotification, object: nil)
        NotificationCenter.default.addObserver(self, selector: #selector(ServerManager.willEnterForeground(_:)), name: UIApplication.willEnterForegroundNotification, object: nil)
    }
}

public extension ServerManager
{
    @objc
    func startDiscovering()
    {
        guard !self.isDiscovering else { return }
        self.isDiscovering = true
        
        DispatchQueue.main.async {
            // NetServiceBrowser must be initialized on main thread.
            // https://stackoverflow.com/questions/3526661/nsnetservicebrowser-delegate-not-called-when-searching
            
            let serviceBrowser = NetServiceBrowser()
            serviceBrowser.delegate = self
            serviceBrowser.includesPeerToPeer = false
            serviceBrowser.searchForServices(ofType: ALTServerServiceType, inDomain: "")
            
            self.serviceBrowser = serviceBrowser
        }
    }
    
    @objc
    func stopDiscovering()
    {
        guard self.isDiscovering else { return }
        self.isDiscovering = false
        
        self.discoveredServers.removeAll()
        self.ignoredServers.removeAll()
        self.resolvingServices.removeAll()
        
        self.serviceBrowser?.stop()
        self.serviceBrowser = nil
    }
    
    func connect(to server: Server, completion: @escaping (Result<ServerConnection, Error>) -> Void)
    {
        var didFinish = false
        
        func finish(_ result: Result<ServerConnection, Error>)
        {
            guard !didFinish else { return }
            didFinish = true
            
            self.ignoredServers.insert(server)
            
            self.callbackQueue.async {
                completion(result)
            }
        }
        
        self.dispatchQueue.async {
            guard #available(iOS 12, tvOS 12, watchOS 5, macOS 10.14, *) else {
                finish(.failure(ConnectionError.unsupportedOS))
                return
            }
            print("Connecting to service:", server.service)
            
            let connection = NWConnection(to: .service(name: server.service.name, type: server.service.type, domain: server.service.domain, interface: nil), using: .tcp)
            connection.stateUpdateHandler = { [unowned connection] (state) in
                switch state
                {
                case .failed(let error):
                    print("Failed to connect to service \(server.service.name).", error)
                    finish(.failure(ConnectionError.connectionFailed(server)))
                    
                case .cancelled: finish(.failure(CocoaError(.userCancelled)))
                    
                case .ready:
                    let networkConnection = NetworkConnection(connection)
                    let serverConnection = ServerConnection(server: server, connection: networkConnection)
                    finish(.success(serverConnection))
                    
                case .waiting: break
                case .setup: break
                case .preparing: break
                @unknown default: break
                }
            }
            
            connection.start(queue: self.dispatchQueue)
        }
    }
    
    func autoconnect(completion: @escaping (Result<ServerConnection, Error>) -> Void)
    {
        self.dispatchQueue.async {
            if case let availableServers = self.discoveredServers.filter({ !self.ignoredServers.contains($0) }),
               let server = availableServers.first(where: { $0.isPreferred }) ?? availableServers.first
            {
                return self.connect(to: server, completion: completion)
            }
            
            self.autoconnectGroup = DispatchGroup()
            self.autoconnectGroup?.enter()
            self.autoconnectGroup?.notify(queue: self.dispatchQueue) {
                self.autoconnectGroup = nil
                
                guard
                    case let availableServers = self.discoveredServers.filter({ !self.ignoredServers.contains($0) }),
                    let server = availableServers.first(where: { $0.isPreferred }) ?? availableServers.first
                else { return self.autoconnect(completion: completion)  }
                
                self.connect(to: server, completion: completion)
            }
        }
    }
}

public extension ServerManager
{
    @objc(sharedManager)
    class var __shared: ServerManager {
        return ServerManager.shared
    }
    
    @objc(connectToServer:completionHandler:)
    func __connect(to server: Server, completion: @escaping (ServerConnection?, Error?) -> Void)
    {
        self.connect(to: server) { result in
            completion(result.value, result.error)
        }
    }
    
    @objc(autoconnectWithCompletionHandler:)
    func __autoconnect(completion: @escaping (ServerConnection?, Error?) -> Void)
    {
        self.autoconnect { result in
            completion(result.value, result.error)
        }
    }
}

private extension ServerManager
{
    func addDiscoveredServer(_ server: Server)
    {
        self.dispatchQueue.async {
            let serverID = Bundle.main.object(forInfoDictionaryKey: "ALTServerID") as? String
            server.isPreferred = (server.id == serverID)
            
            guard !self.discoveredServers.contains(server) else { return }
            
            self.discoveredServers.append(server)
            
            if let callback = self.discoveredServerHandler
            {
                self.callbackQueue.async {
                    callback(server)
                }
            }
        }
    }
    
    func removeDiscoveredServer(_ server: Server)
    {
        self.dispatchQueue.async {
            guard let index = self.discoveredServers.firstIndex(of: server) else { return }
            
            self.discoveredServers.remove(at: index)
            
            if let callback = self.lostServerHandler
            {
                self.callbackQueue.async {
                    callback(server)
                }
            }
        }
    }
}

@objc
private extension ServerManager
{
    @objc
    func didEnterBackground(_ notification: Notification)
    {
        guard self.isDiscovering else { return }
        
        self.resolvingServices.removeAll()
        self.discoveredServers.removeAll()
        self.serviceBrowser?.stop()
    }
    
    @objc
    func willEnterForeground(_ notification: Notification)
    {
        guard self.isDiscovering else { return }
        
        self.serviceBrowser?.searchForServices(ofType: ALTServerServiceType, inDomain: "")
    }
}

extension ServerManager: NetServiceBrowserDelegate
{
    public func netServiceBrowserWillSearch(_ browser: NetServiceBrowser)
    {
        print("Discovering servers...")
    }
    
    public func netServiceBrowserDidStopSearch(_ browser: NetServiceBrowser)
    {
        print("Stopped discovering servers.")
    }
    
    public func netServiceBrowser(_ browser: NetServiceBrowser, didNotSearch errorDict: [String : NSNumber])
    {
        print("Failed to discover servers.", errorDict)
    }
    
    public func netServiceBrowser(_ browser: NetServiceBrowser, didFind service: NetService, moreComing: Bool)
    {
        self.dispatchQueue.async {
            service.delegate = self
            
            if let txtData = service.txtRecordData(), let server = Server(service: service, txtData: txtData)
            {
                self.addDiscoveredServer(server)
            }
            else
            {
                service.resolve(withTimeout: 3.0)
                self.resolvingServices.insert(service)
            }
            
            self.autoconnectGroup?.enter()
            
            if !moreComing
            {
                self.autoconnectGroup?.leave()
            }
        }
    }
    
    public func netServiceBrowser(_ browser: NetServiceBrowser, didRemove service: NetService, moreComing: Bool)
    {
        if let server = self.discoveredServers.first(where: { $0.service == service })
        {
            self.removeDiscoveredServer(server)
        }
    }
}

extension ServerManager: NetServiceDelegate
{
    public func netServiceDidResolveAddress(_ service: NetService)
    {
        defer {
            self.dispatchQueue.async {
                guard self.resolvingServices.contains(service) else { return }
                self.resolvingServices.remove(service)
                
                self.autoconnectGroup?.leave()
            }
        }
        
        guard let data = service.txtRecordData(), let server = Server(service: service, txtData: data) else { return }
        self.addDiscoveredServer(server)
    }
    
    public func netService(_ sender: NetService, didNotResolve errorDict: [String : NSNumber])
    {
        print("Error resolving net service \(sender).", errorDict)
        
        self.dispatchQueue.async {
            guard self.resolvingServices.contains(sender) else { return }
            self.resolvingServices.remove(sender)
            
            self.autoconnectGroup?.leave()
        }
    }
    
    public func netService(_ sender: NetService, didUpdateTXTRecord data: Data)
    {
        let txtDict = NetService.dictionary(fromTXTRecord: data)
        print("Service \(sender) updated TXT Record:", txtDict)
    }
    
    public func netServiceDidStop(_ sender: NetService)
    {
        self.dispatchQueue.async {
            guard self.resolvingServices.contains(sender) else { return }
            self.resolvingServices.remove(sender)
            
            self.autoconnectGroup?.leave()
        }
    }
}
