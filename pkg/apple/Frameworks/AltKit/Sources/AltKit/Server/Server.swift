//
//  Server.swift
//  AltStore
//
//  Created by Riley Testut on 6/20/19.
//  Copyright © 2019 Riley Testut. All rights reserved.
//

import Foundation

@objc(ALTServer)
public class Server: NSObject, Identifiable
{
    public let id: String
    public let service: NetService
    
    public var name: String? {
        return self.service.hostName
    }
    
    public internal(set) var isPreferred = false
    
    public override var hash: Int {
        return self.id.hashValue ^ self.service.name.hashValue
    }
    
    init?(service: NetService, txtData: Data)
    {
        let txtDictionary = NetService.dictionary(fromTXTRecord: txtData)
        guard let identifierData = txtDictionary["serverID"], let identifier = String(data: identifierData, encoding: .utf8) else { return nil }
        
        self.id = identifier
        self.service = service
        
        super.init()
    }
    
    public override func isEqual(_ object: Any?) -> Bool
    {
        guard let server = object as? Server else { return false }
        
        return self.id == server.id && self.service.name == server.service.name // service.name is consistent, and is not the human readable name (hostName).
    }
}
