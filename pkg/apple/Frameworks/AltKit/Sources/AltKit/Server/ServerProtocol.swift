//
//  ServerProtocol.swift
//  AltServer
//
//  Created by Riley Testut on 5/24/19.
//  Copyright © 2019 Riley Testut. All rights reserved.
//

import Foundation

public let ALTServerServiceType = "_altserver._tcp"

protocol ServerMessageProtocol: Codable
{
    var version: Int { get }
    var identifier: String { get }
}

public enum ServerRequest: Decodable
{
    case enableUnsignedCodeExecution(EnableUnsignedCodeExecutionRequest)
    case unknown(identifier: String, version: Int)
    
    var identifier: String {
        switch self
        {
        case .enableUnsignedCodeExecution(let request): return request.identifier
        case .unknown(let identifier, _): return identifier
        }
    }
    
    var version: Int {
        switch self
        {
        case .enableUnsignedCodeExecution(let request): return request.version
        case .unknown(_, let version): return version
        }
    }
    
    private enum CodingKeys: String, CodingKey
    {
        case identifier
        case version
    }
    
    public init(from decoder: Decoder) throws
    {
        let container = try decoder.container(keyedBy: CodingKeys.self)
        
        let version = try container.decode(Int.self, forKey: .version)
        
        let identifier = try container.decode(String.self, forKey: .identifier)
        switch identifier
        {
        case "EnableUnsignedCodeExecutionRequest":
            let request = try EnableUnsignedCodeExecutionRequest(from: decoder)
            self = .enableUnsignedCodeExecution(request)
            
        default:
            self = .unknown(identifier: identifier, version: version)
        }
    }
}

public enum ServerResponse: Decodable
{
    case enableUnsignedCodeExecution(EnableUnsignedCodeExecutionResponse)
    case error(ErrorResponse)
    case unknown(identifier: String, version: Int)
    
    var identifier: String {
        switch self
        {
        case .enableUnsignedCodeExecution(let response): return response.identifier
        case .error(let response): return response.identifier
        case .unknown(let identifier, _): return identifier
        }
    }
    
    var version: Int {
        switch self
        {
        case .enableUnsignedCodeExecution(let response): return response.version
        case .error(let response): return response.version
        case .unknown(_, let version): return version
        }
    }
    
    private enum CodingKeys: String, CodingKey
    {
        case identifier
        case version
    }
    
    public init(from decoder: Decoder) throws
    {
        let container = try decoder.container(keyedBy: CodingKeys.self)
        
        let version = try container.decode(Int.self, forKey: .version)
        
        let identifier = try container.decode(String.self, forKey: .identifier)
        switch identifier
        {
        case "EnableUnsignedCodeExecutionResponse":
            let response = try EnableUnsignedCodeExecutionResponse(from: decoder)
            self = .enableUnsignedCodeExecution(response)
            
        case "ErrorResponse":
            let response = try ErrorResponse(from: decoder)
            self = .error(response)
            
        default:
            self = .unknown(identifier: identifier, version: version)
        }
    }
}

// _Don't_ provide generic SuccessResponse, as that would prevent us
// from easily changing response format for a request in the future.
public struct ErrorResponse: ServerMessageProtocol
{
    public var version = 2
    public var identifier = "ErrorResponse"
    
    public var error: ALTServerError {
        return self.serverError?.error ?? ALTServerError(self.errorCode)
    }
    private var serverError: CodableServerError?
    
    // Legacy (v1)
    private var errorCode: ALTServerError.Code
    
    public init(error: ALTServerError)
    {
        self.serverError = CodableServerError(error: error)
        self.errorCode = error.code
    }
}

public struct EnableUnsignedCodeExecutionRequest: ServerMessageProtocol
{
    public var version = 1
    public var identifier = "EnableUnsignedCodeExecutionRequest"
    
    public var udid: String
    public var processID: Int32?
    public var processName: String?

    public init(udid: String, processID: Int32? = nil, processName: String? = nil)
    {
        self.udid = udid
        self.processID = processID
        self.processName = processName
    }
}

public struct EnableUnsignedCodeExecutionResponse: ServerMessageProtocol
{
    public var version = 1
    public var identifier = "EnableUnsignedCodeExecutionResponse"
    
    public init()
    {
    }
}
