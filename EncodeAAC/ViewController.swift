//
//  ViewController.swift
//  AudioResample
//
//  Created by lichao on 2020/2/9.
//  Copyright © 2020年 lichao. All rights reserved.
//

import Cocoa

class ViewController: NSViewController {
    
    var recStatus: Bool = false;
    var audioThread: Thread?
    var videoThread: Thread?
    var thread: Thread?
    let btn = NSButton.init(title:"", target:nil, action:nil)
    let btn1 = NSButton.init(title:"", target:nil, action:nil)

    override func viewDidLoad() {
        super.viewDidLoad()
        
        // Do any additional setup after loading the view.
        self.view.setFrameSize(NSSize(width: 320, height: 240))
        
        btn.title = "开始录制"
        btn.bezelStyle = .rounded
        btn.setButtonType(.pushOnPushOff)
        btn.frame = NSRect(x:320/2-60,y: 240/2-15, width:120, height: 30)
        btn.target = self
        btn.action = #selector(myfunc)
        
        self.view.addSubview(btn)
        
        btn1.title = "Pcm"
        btn1.bezelStyle = .rounded
        btn1.setButtonType(.pushOnPushOff)
        btn1.frame = NSRect(x:320/2-60,y: 240/2-15+40, width:120, height: 30)
        btn1.target = self
        btn1.action = #selector(myfunc1)
        
        self.view.addSubview(btn1)
        // Do any additional setup after loading the view.
    }
    
    @objc func myfunc(){
        //print("callback!")
        self.recStatus = !self.recStatus;
        
        if recStatus {
//            audioThread = Thread.init(target: self,
//                                     selector: #selector(funcAudio),
//                                     object: nil)
//            audioThread?.start()
            
            videoThread = Thread.init(target: self,
                                      selector: #selector(funcVideo),
                                      object: nil)
            videoThread?.start()
            self.btn.title = "停止录制"
            setStatus(1);
        }else{
            setStatus(0);
            self.btn.title = "开始录制"
        }
    }
    
    @objc func myfunc1(){
        self.recStatus = !self.recStatus;
        
        if recStatus {
            thread = Thread.init(target: self,
                                 selector: #selector(self.recAudio1),
                                 object: nil)
            thread?.start()
            
            self.btn1.title = "停止录制Pcm"
            setStatus(1);
        }else{
            setStatus(0);
            self.btn1.title = "开始录制Pcm"
        }
    }
    @objc func recAudio1(){
//        play()
        startVideo();

    }
    @objc func funcAudio(){
//        startAudio();
    }
    @objc func funcVideo(){
        startMuxer();
//        simplest_aac_parser();
//        play();
    }
    override var representedObject: Any? {
        didSet {
        // Update the view, if already loaded.
        }
    }


}

