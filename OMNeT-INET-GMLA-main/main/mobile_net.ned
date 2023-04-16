import inet.networklayer.configurator.ipv4.Ipv4NetworkConfigurator;
import inet.node.inet.StandardHost;
import inet.physicallayer.wireless.ieee802154.packetlevel.Ieee802154NarrowbandScalarRadioMedium;
import inet.physicallayer.wireless.ieee802154.packetlevel.Ieee802154NarrowbandScalarRadio;
import inet.linklayer.ieee802154.Ieee802154NarrowbandInterface;
import inet.visualizer.canvas.integrated.IntegratedMultiCanvasVisualizer;
import inet.visualizer.canvas.integrated.IntegratedMultiCanvasVisualizer;
import inet.mobility.single.GaussMarkovMobility;

network IEEE_802154_Mobile_Net
{
    parameters:
        @display("bgb=1100,1100;bgg=100,1,grey95");
        @figure[title](type=label; pos=0,-1; anchor=sw; color=darkblue);
        int num = default(65);
    submodules:
        configurator: Ipv4NetworkConfigurator {
            @display("p=950,550");
        }
        radioMedium: Ieee802154NarrowbandScalarRadioMedium {
            @display("p=950,250");
        }
        slave[num]: StandardHost {
            parameters:
                @display("i=misc/node2_vs");
                numWlanInterfaces = default(1);
                wlan[*].typename = "Ieee802154NarrowbandInterface";
        }
        master: StandardHost {
            parameters:
                @display("p=550,550");
                numWlanInterfaces = default(1);
                wlan[*].typename = "Ieee802154NarrowbandInterface";
        }
        integratedMultiCanvasVisualizer: IntegratedMultiCanvasVisualizer {
            @display("p=883.19995,896.448");
        }
}
