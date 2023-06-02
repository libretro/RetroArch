//
//  NetworkConnection.swift
//  AltKit
//
//  Created by Riley Testut on 6/1/20.
//  Copyright © 2020 Riley Testut. All rights reserved.
//

import Foundation
import Network

@available(iOS 12, tvOS 12, watchOS 5, macOS 10.14, *)
public class NetworkConnection: NSObject, Connection
{
    public let nwConnection: NWConnection
    
    public init(_ nwConnection: NWConnection)
    {
        self.nwConnection = nwConnection
    }
    
    public func send(_ data: Data, completionHandler: @escaping (Result<Void, ALTServerError>) -> Void)
    {
        self.nwConnection.send(content: data, completion: .contentProcessed { (error) in
            if let error = error
            {
                completionHandler(.failure(.init(.lostConnection, underlyingError: error)))
            }
            else
            {
                completionHandler(.success(()))
            }
        })
    }
    
    public func receiveData(expectedSize: Int, completionHandler: @escaping (Result<Data, ALTServerError>) -> Void)
    {
        self.nwConnection.receive(minimumIncompleteLength: expectedSize, maximumLength: expectedSize) { (data, context, isComplete, error) in
            switch (data, error)
            {
            case (let data?, _): completionHandler(.success(data))
            case (_, let error?): completionHandler(.failure(.init(.lostConnection, underlyingError: error)))
            case (nil, nil): completionHandler(.failure(ALTServerError(.lostConnection)))
            }
        }
    }
    
    public func disconnect()
    {
        switch self.nwConnection.state
        {
        case .cancelled, .failed: break
        default: self.nwConnection.cancel()
        }
    }
    
    override public var description: String {
        return "\(self.nwConnection.endpoint) (Network)"
    }
}
