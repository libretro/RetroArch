//
//  Connection.swift
//  AltKit
//
//  Created by Riley Testut on 6/1/20.
//  Copyright © 2020 Riley Testut. All rights reserved.
//

import Foundation
import Network

public protocol Connection
{
    func send(_ data: Data, completionHandler: @escaping (Result<Void, ALTServerError>) -> Void)
    func receiveData(expectedSize: Int, completionHandler: @escaping (Result<Data, ALTServerError>) -> Void)
    
    func disconnect()
}
